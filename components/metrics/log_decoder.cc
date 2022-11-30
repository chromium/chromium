// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/log_decoder.h"

#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "third_party/zlib/google/compression_utils.h"

namespace metrics {

bool DecodeLogData(const std::string& compressed_log_data,
                   std::string* log_data) {
  return compression::GzipUncompress(compressed_log_data, log_data);
}

bool DecodeLogDataToProto(const std::string& compressed_log_data,
                          google::protobuf::MessageLite* proto) {
  std::string log_data;
  if (!DecodeLogData(compressed_log_data, &log_data))
    return false;

  return proto->ParseFromString(log_data);
}

}  // namespace metrics
