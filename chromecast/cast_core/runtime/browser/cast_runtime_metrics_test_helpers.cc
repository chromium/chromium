// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/cast_runtime_metrics_test_helpers.h"

namespace chromecast {

int64_t GetCount(int64_t value, const cast::metrics::Histogram* histogram) {
  for (int i = 0; i < histogram->bucket_size(); ++i) {
    const auto& bucket = histogram->bucket(i);
    if (value >= bucket.min() && value < bucket.max()) {
      return bucket.count();
    }
  }
  return 0;
}

std::unique_ptr<::metrics::CastLogsProto_CastEventProto> ParseCastEventProto(
    const std::string& serialized) {
  auto event = std::make_unique<::metrics::CastLogsProto_CastEventProto>();
  if (!event->ParseFromString(serialized)) {
    return nullptr;
  }
  return event;
}

}  // namespace chromecast
