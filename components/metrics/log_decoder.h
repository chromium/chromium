// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LOG_DECODER_H_
#define COMPONENTS_METRICS_LOG_DECODER_H_

#include <string>

namespace google {
namespace protobuf {

class MessageLite;

}  // namespace protobuf
}  // namespace google

namespace metrics {

// Other modules can call this function instead of directly calling gzip. This
// prevents other modules from having to depend on zlib, or being aware of
// metrics' use of gzip compression, which is a metrics implementation detail.
// Returns true on success, false on failure.
bool DecodeLogData(const std::string& compressed_log_data,
                   std::string* log_data);

// Decodes |compressed_log_data| and populates |proto| with the decompressed log
// data. Returns true on success and false on failure.
bool DecodeLogDataToProto(const std::string& compressed_log_data,
                          google::protobuf::MessageLite* proto);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_LOG_DECODER_H_
