// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_RECEIVER_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_RECEIVER_SESSION_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/renderer/media/cast_receiver_audio_valve.h"
#include "chrome/renderer/media/cast_session_delegate.h"
#include "media/capture/video_capture_types.h"
#include "media/cast/cast_receiver.h"
#include "third_party/blink/public/common/media/video_capture.h"

class CastReceiverSessionDelegate : public CastSessionDelegateBase {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  CastReceiverSessionDelegate();
  ~CastReceiverSessionDelegate() override;

  void ReceivePacket(std::unique_ptr<media::cast::Packet> packet) override;

  void Start(const media::cast::FrameReceiverConfig& audio_config,
             const media::cast::FrameReceiverConfig& video_config,
             const net::IPEndPoint& local_endpoint,
             const net::IPEndPoint& remote_endpoint,
             std::unique_ptr<base::DictionaryValue> options,
             const media::VideoCaptureFormat& format,
             const ErrorCallback& error_callback);

  void StartAudio(scoped_refptr<CastReceiverAudioValve> audio_valve);

  void StartVideo(blink::VideoCaptureDeliverFrameCB frame_callback);
  // Stop Video callbacks (eventually).
  void StopVideo();

 private:
  void OnDecodedAudioFrame(std::unique_ptr<media::AudioBus> audio_bus,
                           base::TimeTicks playout_time,
                           bool is_continuous);

  void OnDecodedVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                           base::TimeTicks playout_time,
                           bool is_continuous);

  scoped_refptr<CastReceiverAudioValve> audio_valve_;
  blink::VideoCaptureDeliverFrameCB frame_callback_;
  media::cast::AudioFrameDecodedCallback on_audio_decoded_cb_;
  media::cast::VideoFrameDecodedCallback on_video_decoded_cb_;
  std::unique_ptr<media::cast::CastReceiver> cast_receiver_;
  media::VideoCaptureFormat format_;
  base::WeakPtrFactory<CastReceiverSessionDelegate> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CastReceiverSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_RECEIVER_SESSION_DELEGATE_H_
