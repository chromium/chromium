// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_transport_ipc.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/containers/id_map.h"
#include "chrome/common/cast_messages.h"
#include "chrome/renderer/media/cast_ipc_dispatcher.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/cast_sender.h"

CastTransportIPC::CastTransportIPC(
    const net::IPEndPoint& local_end_point,
    const net::IPEndPoint& remote_end_point,
    std::unique_ptr<base::DictionaryValue> options,
    const media::cast::PacketReceiverCallback& packet_callback,
    const media::cast::CastTransportStatusCallback& status_cb,
    const media::cast::BulkRawEventsCallback& raw_events_cb)
    : channel_id_(-1),
      packet_callback_(packet_callback),
      status_callback_(status_cb),
      raw_events_callback_(raw_events_cb) {
  if (CastIPCDispatcher::Get()) {
    // TODO(miu): CastIPCDispatcher should be provided as a ctor argument.
    channel_id_ = CastIPCDispatcher::Get()->AddSender(this);
    Send(new CastHostMsg_New(channel_id_, local_end_point, remote_end_point,
                             *options));
  }
}

CastTransportIPC::~CastTransportIPC() {
  Send(new CastHostMsg_Delete(channel_id_));
  if (CastIPCDispatcher::Get()) {
    CastIPCDispatcher::Get()->RemoveSender(channel_id_);
  }
}

void CastTransportIPC::InitializeStream(
    const media::cast::CastTransportRtpConfig& config,
    std::unique_ptr<media::cast::RtcpObserver> rtcp_observer) {
  if (rtcp_observer) {
    DCHECK(clients_.find(config.ssrc) == clients_.end());
    clients_[config.ssrc] = std::move(rtcp_observer);
  }
  Send(new CastHostMsg_InitializeStream(channel_id_, config));
}

void CastTransportIPC::InsertFrame(uint32_t ssrc,
                                   const media::cast::EncodedFrame& frame) {
  Send(new CastHostMsg_InsertFrame(channel_id_, ssrc, frame));
}

void CastTransportIPC::SendSenderReport(
    uint32_t ssrc,
    base::TimeTicks current_time,
    media::cast::RtpTimeTicks current_time_as_rtp_timestamp) {
  Send(new CastHostMsg_SendSenderReport(channel_id_, ssrc, current_time,
                                        current_time_as_rtp_timestamp));
}

void CastTransportIPC::CancelSendingFrames(
    uint32_t ssrc,
    const std::vector<media::cast::FrameId>& frame_ids) {
  Send(new CastHostMsg_CancelSendingFrames(channel_id_, ssrc, frame_ids));
}

void CastTransportIPC::ResendFrameForKickstart(uint32_t ssrc,
                                               media::cast::FrameId frame_id) {
  Send(new CastHostMsg_ResendFrameForKickstart(channel_id_, ssrc, frame_id));
}

void CastTransportIPC::AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                                           uint32_t rtp_receiver_ssrc) {
  Send(new CastHostMsg_AddValidRtpReceiver(channel_id_, rtp_sender_ssrc,
                                           rtp_receiver_ssrc));
}

void CastTransportIPC::InitializeRtpReceiverRtcpBuilder(
    uint32_t rtp_receiver_ssrc,
    const media::cast::RtcpTimeData& time_data) {
  Send(new CastHostMsg_InitializeRtpReceiverRtcpBuilder(
      channel_id_, rtp_receiver_ssrc, time_data));
}

void CastTransportIPC::AddCastFeedback(
    const media::cast::RtcpCastMessage& cast_message,
    base::TimeDelta target_delay) {
  Send(
      new CastHostMsg_AddCastFeedback(channel_id_, cast_message, target_delay));
}

void CastTransportIPC::AddPli(const media::cast::RtcpPliMessage& pli_message) {
  Send(new CastHostMsg_AddPli(channel_id_, pli_message));
}

void CastTransportIPC::AddRtcpEvents(
    const media::cast::ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) {
  Send(new CastHostMsg_AddRtcpEvents(channel_id_, rtcp_events));
}

void CastTransportIPC::AddRtpReceiverReport(
    const media::cast::RtcpReportBlock& rtp_receiver_report_block) {
  Send(new CastHostMsg_AddRtpReceiverReport(channel_id_,
                                            rtp_receiver_report_block));
}

void CastTransportIPC::SendRtcpFromRtpReceiver() {
  Send(new CastHostMsg_SendRtcpFromRtpReceiver(channel_id_));
}

void CastTransportIPC::OnNotifyStatusChange(
    media::cast::CastTransportStatus status) {
  status_callback_.Run(status);
}

void CastTransportIPC::OnRawEvents(
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  // Note: Casting away const to avoid having to copy all the data elements.  As
  // the only consumer of this data in the IPC message, mutating the inputs
  // should be acceptable.  Just nod and blame the interface we were given here.
  std::unique_ptr<std::vector<media::cast::FrameEvent>> taken_frame_events(
      new std::vector<media::cast::FrameEvent>());
  taken_frame_events->swap(
      const_cast<std::vector<media::cast::FrameEvent>&>(frame_events));
  std::unique_ptr<std::vector<media::cast::PacketEvent>> taken_packet_events(
      new std::vector<media::cast::PacketEvent>());
  taken_packet_events->swap(
      const_cast<std::vector<media::cast::PacketEvent>&>(packet_events));
  raw_events_callback_.Run(std::move(taken_frame_events),
                           std::move(taken_packet_events));
}

void CastTransportIPC::OnRtt(uint32_t rtp_sender_ssrc, base::TimeDelta rtt) {
  auto it = clients_.find(rtp_sender_ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received RTT report for unknown SSRC: " << rtp_sender_ssrc;
    return;
  }
  it->second->OnReceivedRtt(rtt);
}

void CastTransportIPC::OnRtcpCastMessage(
    uint32_t rtp_sender_ssrc,
    const media::cast::RtcpCastMessage& cast_message) {
  auto it = clients_.find(rtp_sender_ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received cast message for unknown SSRC: " << rtp_sender_ssrc;
    return;
  }
  it->second->OnReceivedCastMessage(cast_message);
}

void CastTransportIPC::OnReceivedPli(uint32_t rtp_sender_ssrc) {
  auto it = clients_.find(rtp_sender_ssrc);
  if (it == clients_.end()) {
    LOG(ERROR) << "Received picture loss indicator for unknown SSRC: "
               << rtp_sender_ssrc;
    return;
  }
  it->second->OnReceivedPli();
}

void CastTransportIPC::OnReceivedPacket(const media::cast::Packet& packet) {
  if (!packet_callback_.is_null()) {
    // TODO(hubbe): Perhaps an non-ownership-transferring cb here?
    std::unique_ptr<media::cast::Packet> packet_copy(
        new media::cast::Packet(packet));
    packet_callback_.Run(std::move(packet_copy));
  } else {
    DVLOG(1) << "CastIPCDispatcher::OnReceivedPacket no packet callback yet.";
  }
}

void CastTransportIPC::Send(IPC::Message* message) {
  if (CastIPCDispatcher::Get() && channel_id_ != -1) {
    CastIPCDispatcher::Get()->Send(message);
  } else {
    delete message;
  }
}
