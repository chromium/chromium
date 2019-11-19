// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_session_delegate.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/renderer/media/cast_threads.h"
#include "chrome/renderer/media/cast_transport_ipc.h"
#include "components/version_info/version_info.h"
#include "content/public/renderer/render_thread.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"

using media::cast::CastEnvironment;
using media::cast::CastSender;
using media::cast::FrameSenderConfig;

static base::LazyInstance<CastThreads>::DestructorAtExit g_cast_threads =
    LAZY_INSTANCE_INITIALIZER;

CastSessionDelegateBase::CastSessionDelegateBase()
    : io_task_runner_(content::RenderThread::Get()->GetIOTaskRunner()) {
  DCHECK(io_task_runner_.get());
#if defined(OS_WIN)
  // Note that this also increases the accuracy of PostDelayTask,
  // which is is very helpful to cast.
  if (!base::Time::ActivateHighResolutionTimer(true)) {
    LOG(WARNING) << "Failed to activate high resolution timers for cast.";
  }
#endif
}

CastSessionDelegateBase::~CastSessionDelegateBase() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
#if defined(OS_WIN)
  base::Time::ActivateHighResolutionTimer(false);
#endif
}

void CastSessionDelegateBase::StartUDP(
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    std::unique_ptr<base::DictionaryValue> options,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // CastSender uses the renderer's IO thread as the main thread. This reduces
  // thread hopping for incoming video frames and outgoing network packets.
  // TODO(hubbe): Create cast environment in ctor instead.
  cast_environment_ =
      new CastEnvironment(base::DefaultTickClock::GetInstance(),
                          base::ThreadTaskRunnerHandle::Get(),
                          g_cast_threads.Get().GetAudioEncodeTaskRunner(),
                          g_cast_threads.Get().GetVideoEncodeTaskRunner());

  // Rationale for using unretained: The callback cannot be called after the
  // destruction of CastTransportIPC, and they both share the same thread.
  cast_transport_.reset(new CastTransportIPC(
      local_endpoint, remote_endpoint, std::move(options),
      base::Bind(&CastSessionDelegateBase::ReceivePacket,
                 base::Unretained(this)),
      base::Bind(&CastSessionDelegateBase::StatusNotificationCB,
                 base::Unretained(this), error_callback),
      base::Bind(&media::cast::LogEventDispatcher::DispatchBatchOfEvents,
                 base::Unretained(cast_environment_->logger()))));
}

void CastSessionDelegateBase::StatusNotificationCB(
    const ErrorCallback& error_callback,
    media::cast::CastTransportStatus status) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::string error_message;

  switch (status) {
    case media::cast::TRANSPORT_STREAM_UNINITIALIZED:
    case media::cast::TRANSPORT_STREAM_INITIALIZED:
      return; // Not errors, do nothing.
    case media::cast::TRANSPORT_INVALID_CRYPTO_CONFIG:
      error_callback.Run("Invalid encrypt/decrypt configuration.");
      break;
    case media::cast::TRANSPORT_SOCKET_ERROR:
      error_callback.Run("Socket error.");
      break;
  }
}

CastSessionDelegate::CastSessionDelegate() {
  DCHECK(io_task_runner_.get());
}

CastSessionDelegate::~CastSessionDelegate() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

void CastSessionDelegate::StartAudio(
    const FrameSenderConfig& config,
    const AudioFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  audio_frame_input_available_callback_ = callback;
  cast_sender_->InitializeAudio(
      config,
      base::Bind(&CastSessionDelegate::OnOperationalStatusChange,
                 weak_factory_.GetWeakPtr(), true, error_callback));
}

void CastSessionDelegate::StartVideo(
    const FrameSenderConfig& config,
    const VideoFrameInputAvailableCallback& callback,
    const ErrorCallback& error_callback,
    const media::cast::CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    const media::cast::CreateVideoEncodeMemoryCallback&
        create_video_encode_mem_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!cast_transport_ || !cast_sender_) {
    error_callback.Run("Destination not set.");
    return;
  }

  video_frame_input_available_callback_ = callback;

  cast_sender_->InitializeVideo(
      config,
      base::Bind(&CastSessionDelegate::OnOperationalStatusChange,
                 weak_factory_.GetWeakPtr(), false, error_callback),
      create_vea_cb,
      create_video_encode_mem_cb);
}

void CastSessionDelegate::StartRemotingStream(
    int32_t stream_id,
    const FrameSenderConfig& config,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!cast_transport_) {
    error_callback.Run("Destination not set.");
    return;
  }

  media::cast::CastTransportRtpConfig transport_config;
  transport_config.ssrc = config.sender_ssrc;
  transport_config.feedback_ssrc = config.receiver_ssrc;
  transport_config.rtp_payload_type = config.rtp_payload_type;
  transport_config.rtp_stream_id = stream_id;
  transport_config.aes_key = config.aes_key;
  transport_config.aes_iv_mask = config.aes_iv_mask;
  cast_transport_->InitializeStream(transport_config, nullptr);
}

void CastSessionDelegate::StartUDP(
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    std::unique_ptr<base::DictionaryValue> options,
    const ErrorCallback& error_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  CastSessionDelegateBase::StartUDP(local_endpoint, remote_endpoint,
                                    std::move(options), error_callback);
  event_subscribers_.reset(
      new media::cast::RawEventSubscriberBundle(cast_environment_));

  cast_sender_ = CastSender::Create(cast_environment_, cast_transport_.get());
}

void CastSessionDelegate::ToggleLogging(bool is_audio, bool enable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!event_subscribers_.get())
    return;

  if (enable)
    event_subscribers_->AddEventSubscribers(is_audio);
  else
    event_subscribers_->RemoveEventSubscribers(is_audio);
}

void CastSessionDelegate::GetEventLogsAndReset(
    bool is_audio,
    const std::string& extra_data,
    const EventLogsCallback& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!event_subscribers_.get()) {
    callback.Run(std::make_unique<base::Value>(base::Value::Type::BINARY));
    return;
  }

  media::cast::EncodingEventSubscriber* subscriber =
      event_subscribers_->GetEncodingEventSubscriber(is_audio);
  if (!subscriber) {
    callback.Run(std::make_unique<base::Value>(base::Value::Type::BINARY));
    return;
  }

  media::cast::proto::LogMetadata metadata;
  media::cast::FrameEventList frame_events;
  media::cast::PacketEventList packet_events;

  subscriber->GetEventsAndReset(&metadata, &frame_events, &packet_events);

  if (!extra_data.empty())
    metadata.set_extra_data(extra_data);
  media::cast::proto::GeneralDescription* gen_desc =
      metadata.mutable_general_description();
  gen_desc->set_product(version_info::GetProductName());
  gen_desc->set_product_version(version_info::GetVersionNumber());
  gen_desc->set_os(version_info::GetOSType());

  std::unique_ptr<char[]> serialized_log(
      new char[media::cast::kMaxSerializedBytes]);
  int output_bytes;
  bool success = media::cast::SerializeEvents(metadata,
                                              frame_events,
                                              packet_events,
                                              true,
                                              media::cast::kMaxSerializedBytes,
                                              serialized_log.get(),
                                              &output_bytes);

  if (!success) {
    DVLOG(2) << "Failed to serialize event log.";
    callback.Run(std::make_unique<base::Value>(base::Value::Type::BINARY));
    return;
  }

  DVLOG(2) << "Serialized log length: " << output_bytes;

  auto blob = std::make_unique<base::Value>(std::vector<char>(
      serialized_log.get(), serialized_log.get() + output_bytes));
  callback.Run(std::move(blob));
}

void CastSessionDelegate::GetStatsAndReset(bool is_audio,
                                           const StatsCallback& callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!event_subscribers_.get()) {
    callback.Run(std::make_unique<base::DictionaryValue>());
    return;
  }

  media::cast::StatsEventSubscriber* subscriber =
      event_subscribers_->GetStatsEventSubscriber(is_audio);
  if (!subscriber) {
    callback.Run(std::make_unique<base::DictionaryValue>());
    return;
  }

  std::unique_ptr<base::DictionaryValue> stats = subscriber->GetStats();
  subscriber->Reset();

  callback.Run(std::move(stats));
}

void CastSessionDelegate::OnOperationalStatusChange(
    bool is_for_audio,
    const ErrorCallback& error_callback,
    media::cast::OperationalStatus status) {
  DCHECK(cast_sender_);

  switch (status) {
    case media::cast::STATUS_UNINITIALIZED:
    case media::cast::STATUS_CODEC_REINIT_PENDING:
      // Not an error.
      // TODO(miu): As an optimization, signal the client to pause sending more
      // frames until the state becomes STATUS_INITIALIZED again.
      break;
    case media::cast::STATUS_INITIALIZED:
      // Once initialized, run the "frame input available" callback to allow the
      // client to begin sending frames.  If STATUS_INITIALIZED is encountered
      // again, do nothing since this is only an indication that the codec has
      // successfully re-initialized.
      if (is_for_audio) {
        if (!audio_frame_input_available_callback_.is_null()) {
          std::move(audio_frame_input_available_callback_)
              .Run(cast_sender_->audio_frame_input());
        }
      } else {
        if (!video_frame_input_available_callback_.is_null()) {
          std::move(video_frame_input_available_callback_)
              .Run(cast_sender_->video_frame_input());
        }
      }
      break;
    case media::cast::STATUS_INVALID_CONFIGURATION:
      error_callback.Run(base::StringPrintf("Invalid %s configuration.",
                                            is_for_audio ? "audio" : "video"));
      break;
    case media::cast::STATUS_UNSUPPORTED_CODEC:
      error_callback.Run(base::StringPrintf("%s codec not supported.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
    case media::cast::STATUS_CODEC_INIT_FAILED:
      error_callback.Run(base::StringPrintf("%s codec initialization failed.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
    case media::cast::STATUS_CODEC_RUNTIME_ERROR:
      error_callback.Run(base::StringPrintf("%s codec runtime error.",
                                            is_for_audio ? "Audio" : "Video"));
      break;
  }
}

void CastSessionDelegate::ReceivePacket(
    std::unique_ptr<media::cast::Packet> packet) {
  // Do nothing (frees packet)
}
