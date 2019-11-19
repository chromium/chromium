// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SESSION_H_
#define CHROME_RENDERER_MEDIA_CAST_SESSION_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_config.h"
#include "net/base/ip_endpoint.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class Value;
}  // namespace base

namespace media {
namespace cast {
class AudioFrameInput;
class VideoFrameInput;
}  // namespace cast
}  // namespace media

class CastSessionDelegate;

// This class represents a Cast session and allows the session to be
// configured on the main thread. Actual work is forwarded to
// CastSessionDelegate on the IO thread.
class CastSession : public base::RefCounted<CastSession> {
 public:
  using AudioFrameInputAvailableCallback =
      base::Callback<void(const scoped_refptr<media::cast::AudioFrameInput>&)>;
  using VideoFrameInputAvailableCallback =
      base::Callback<void(const scoped_refptr<media::cast::VideoFrameInput>&)>;
  using SendPacketCallback = base::Callback<void(const std::vector<char>&)>;
  using EventLogsCallback = base::Callback<void(std::unique_ptr<base::Value>)>;
  using StatsCallback =
      base::Callback<void(std::unique_ptr<base::DictionaryValue>)>;
  using ErrorCallback = base::Callback<void(const std::string&)>;

  explicit CastSession(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Start encoding of audio and video using the provided configuration.
  //
  // When Cast sender is started and ready to be used
  // media::cast::FrameInput will be given through |callback|.
  // If it encounters an error, |error_callback| will be invoked with the
  // error message. Both |callback| and |error_callback| will be made on
  // the main thread.
  // |StartUDP()| must be called before these methods.
  void StartAudio(const media::cast::FrameSenderConfig& config,
                  const AudioFrameInputAvailableCallback& callback,
                  const ErrorCallback& error_callback);
  void StartVideo(const media::cast::FrameSenderConfig& config,
                  const VideoFrameInputAvailableCallback& callback,
                  const ErrorCallback& error_callback);

  // Start remoting a stream. |error_callback| will be invoked when any error
  // occurs. |StartUDP()| must be called before calling this method.
  void StartRemotingStream(int32_t stream_id,
                           const media::cast::FrameSenderConfig& config,
                           const ErrorCallback& error_callback);

  // This will create the Cast transport and connect to |remote_endpoint|.
  // |options| is a dictionary which contain optional configuration for the
  // udp transport.
  // Must be called before initialization of audio or video.
  void StartUDP(const net::IPEndPoint& remote_endpoint,
                std::unique_ptr<base::DictionaryValue> options,
                const ErrorCallback& error_callback);

  // Creates or destroys event subscriber for the audio or video stream.
  // |is_audio|: true if the event subscriber is for audio. Video otherwise.
  // |enable|: If true, creates an event subscriber. Otherwise destroys
  // existing subscriber and discards logs.
  void ToggleLogging(bool is_audio, bool enable);

  // Returns raw event logs in serialized format for either the audio or video
  // stream since last call and returns result in |callback|. Also attaches
  // |extra_data| to the log.
  void GetEventLogsAndReset(bool is_audio,
      const std::string& extra_data, const EventLogsCallback& callback);

  // Returns stats in a DictionaryValue format for either the audio or video
  // stream since last call and returns result in |callback|.
  void GetStatsAndReset(bool is_audio, const StatsCallback& callback);

 private:
  friend class base::RefCounted<CastSession>;
  virtual ~CastSession();

  // This member should never be dereferenced on the main thread.
  // CastSessionDelegate lives only on the IO thread. It is always
  // safe to post task on the IO thread to access CastSessionDelegate
  // because it is owned by this object.
  std::unique_ptr<CastSessionDelegate> delegate_;

  // A main thread task runner that might execute JavaScript.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Proxy to the IO task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(CastSession);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SESSION_H_
