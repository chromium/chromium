// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/logging_defines.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class Value;
}  // namespace base

namespace media {

namespace cast {
class CastEnvironment;
class RawEventSubscriberBundle;

namespace transport {
class CastTransport;
}  // namespace transport
}  // namespace cast
}  // namespace media

// Breaks out functionality that is common between CastSessionDelegate and
// CastReceiverSessionDelegate.
class CastSessionDelegateBase {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  CastSessionDelegateBase();
  virtual ~CastSessionDelegateBase();

  // This will start the session by configuring and creating the Cast transport
  // and the Cast sender.
  // Must be called before initialization of audio or video.
  void StartUDP(const net::IPEndPoint& local_endpoint,
                const net::IPEndPoint& remote_endpoint,
                std::unique_ptr<base::DictionaryValue> options,
                const ErrorCallback& error_callback);

 protected:
  void StatusNotificationCB(
      const ErrorCallback& error_callback,
      media::cast::CastTransportStatus status);

  virtual void ReceivePacket(std::unique_ptr<media::cast::Packet> packet) = 0;

  base::ThreadChecker thread_checker_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<media::cast::CastTransport> cast_transport_;

  // Proxy to the IO message loop.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  base::WeakPtrFactory<CastSessionDelegateBase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegateBase);
};

// This class hosts CastSender and connects it to audio/video frame input
// and network socket.
// This class is created on the render thread and destroyed on the IO
// thread. All methods are accessible only on the IO thread.
class CastSessionDelegate : public CastSessionDelegateBase {
 public:
  typedef base::Callback<void(const scoped_refptr<
      media::cast::AudioFrameInput>&)> AudioFrameInputAvailableCallback;
  typedef base::Callback<void(const scoped_refptr<
      media::cast::VideoFrameInput>&)> VideoFrameInputAvailableCallback;
  typedef base::Callback<void(std::unique_ptr<base::Value>)> EventLogsCallback;
  typedef base::Callback<void(std::unique_ptr<base::DictionaryValue>)>
      StatsCallback;

  CastSessionDelegate();
  ~CastSessionDelegate() override;

  void StartUDP(const net::IPEndPoint& local_endpoint,
                const net::IPEndPoint& remote_endpoint,
                std::unique_ptr<base::DictionaryValue> options,
                const ErrorCallback& error_callback);

  // After calling StartAudio() or StartVideo() encoding of that media will
  // begin as soon as data is delivered to its sink, if the second method is
  // called the first media will be restarted. It is strongly recommended not to
  // deliver any data between calling the two methods.
  // It's OK to call only one of the two methods.
  // StartUDP must be called before these methods.
  void StartAudio(const media::cast::FrameSenderConfig& config,
                  const AudioFrameInputAvailableCallback& callback,
                  const ErrorCallback& error_callback);

  void StartVideo(
      const media::cast::FrameSenderConfig& config,
      const VideoFrameInputAvailableCallback& callback,
      const ErrorCallback& error_callback,
      const media::cast::CreateVideoEncodeAcceleratorCallback& create_vea_cb,
      const media::cast::CreateVideoEncodeMemoryCallback&
          create_video_encode_mem_cb);

  // Start remoting session for one stream. After calling this method, a
  // remoting sender will be ready for sending the demuxed stream. StartUDP()
  // must be called before calling this method.
  void StartRemotingStream(int32_t stream_id,
                           const media::cast::FrameSenderConfig& config,
                           const ErrorCallback& error_callback);

  void ToggleLogging(bool is_audio, bool enable);
  void GetEventLogsAndReset(bool is_audio,
      const std::string& extra_data, const EventLogsCallback& callback);
  void GetStatsAndReset(bool is_audio, const StatsCallback& callback);

 protected:
  // Called to report back operational status changes.  The first time this is
  // called with STATUS_INITIALIZED will result in running the "frame input
  // available" callback, to indicate the session is ready to accept incoming
  // audio/video frames.  If this is called with an error that has halted the
  // session, the |error_callback| provided to StartXXX() will be run.  This
  // method may be called multiple times during the session to indicate codec
  // re-initializations are taking place and/or runtime errors have occurred.
  void OnOperationalStatusChange(
      bool is_for_audio,
      const ErrorCallback& error_callback,
      media::cast::OperationalStatus result);

 private:
  void ReceivePacket(std::unique_ptr<media::cast::Packet> packet) override;

  std::unique_ptr<media::cast::CastSender> cast_sender_;

  AudioFrameInputAvailableCallback audio_frame_input_available_callback_;
  VideoFrameInputAvailableCallback video_frame_input_available_callback_;

  std::unique_ptr<media::cast::RawEventSubscriberBundle> event_subscribers_;

  base::WeakPtrFactory<CastSessionDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastSessionDelegate);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_DELEGATE_H_
