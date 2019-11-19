// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
#define CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"
#include "third_party/blink/public/platform/web_media_stream_track.h"

namespace base {
class DictionaryValue;
class Value;
}

class CastAudioSink;
class CastSession;
class CastVideoSink;

// This object represents a RTP stream that encodes and optionally
// encrypt audio or video data from a WebMediaStreamTrack.
// Note that this object does not actually output packets. It allows
// configuration of encoding and RTP parameters and control such a logical
// stream.
class CastRtpStream {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  static bool IsHardwareVP8EncodingSupported();

  static bool IsHardwareH264EncodingSupported();

  CastRtpStream(const blink::WebMediaStreamTrack& track,
                const scoped_refptr<CastSession>& session);
  CastRtpStream(bool is_audio, const scoped_refptr<CastSession>& session);
  ~CastRtpStream();

  // Return parameters currently supported by this stream.
  std::vector<media::cast::FrameSenderConfig> GetSupportedConfigs();

  // Begin encoding of media stream and then submit the encoded streams
  // to underlying transport.
  // |stream_id| is the unique ID of this stream.
  // When the stream is started |start_callback| is called.
  // When the stream is stopped |stop_callback| is called.
  // When there is an error |error_callback| is called with a message.
  void Start(int32_t stream_id,
             const media::cast::FrameSenderConfig& config,
             const base::Closure& start_callback,
             const base::Closure& stop_callback,
             const ErrorCallback& error_callback);

  // Stop encoding.
  void Stop();

  // Enables or disables logging for this stream.
  void ToggleLogging(bool enable);

  // Get serialized raw events for this stream with |extra_data| attached,
  // and invokes |callback| with the result.
  void GetRawEvents(
      const base::Callback<void(std::unique_ptr<base::Value>)>& callback,
      const std::string& extra_data);

  // Get stats in DictionaryValue format and invokves |callback| with
  // the result.
  void GetStats(const base::Callback<
                void(std::unique_ptr<base::DictionaryValue>)>& callback);

 private:
  void DidEncounterError(const std::string& message);

  blink::WebMediaStreamTrack track_;
  const scoped_refptr<CastSession> cast_session_;
  std::unique_ptr<CastAudioSink> audio_sink_;
  std::unique_ptr<CastVideoSink> video_sink_;
  base::Closure stop_callback_;
  ErrorCallback error_callback_;
  bool is_audio_;

  base::WeakPtrFactory<CastRtpStream> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CastRtpStream);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
