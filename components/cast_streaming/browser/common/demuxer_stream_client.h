// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DEMUXER_STREAM_CLIENT_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DEMUXER_STREAM_CLIENT_H_

#include "base/functional/callback.h"

namespace cast_streaming {

// This class acts as a client to act upon state changes of the DemuxerStream
// and DemuxerStreamDataProvider with which it is associated.
class DemuxerStreamClient {
 public:
  virtual ~DemuxerStreamClient() = default;

  // Enables the bitstream converter for the data provider associated with this
  // demuxer stream.
  using BitstreamConverterEnabledCB = base::OnceCallback<void(bool)>;
  virtual void EnableBitstreamConverter(BitstreamConverterEnabledCB cb) = 0;

  // Called when no buffers are available for reading.
  virtual void OnNoBuffersAvailable() = 0;

  // Called when a fatal error occurs. Only called once.
  virtual void OnError() = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_COMMON_DEMUXER_STREAM_CLIENT_H_
