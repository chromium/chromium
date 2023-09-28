// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/web_applications/generated_icon_fix_util.h"

#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "components/sync/base/time.h"

namespace web_app {

bool IsGeneratedIconFixValid(const GeneratedIconFix& generated_icon_fix) {
  return generated_icon_fix.has_source() &&
         generated_icon_fix.source() != GeneratedIconFixSource_UNKNOWN &&
         generated_icon_fix.has_window_start_time() &&
         generated_icon_fix.has_attempt_count();
}

base::Value GeneratedIconFixToDebugValue(
    const GeneratedIconFix* generated_icon_fix) {
  if (!generated_icon_fix) {
    return base::Value();
  }

  base::Value::Dict debug_value;
  debug_value.Set("source", [&] {
    switch (generated_icon_fix->source()) {
      case GeneratedIconFixSource_UNKNOWN:
        NOTREACHED();
        return "Unknown";
      case GeneratedIconFixSource_SYNC_INSTALL:
        return "SyncInstall";
      case GeneratedIconFixSource_RETROACTIVE:
        return "Retroactive";
    }
  }());
  debug_value.Set("window_start_time",
                  base::ToString(syncer::ProtoTimeToTime(
                      generated_icon_fix->window_start_time())));
  debug_value.Set("last_attempt_time",
                  generated_icon_fix->has_last_attempt_time()
                      ? base::Value(base::ToString(syncer::ProtoTimeToTime(
                            generated_icon_fix->last_attempt_time())))
                      : base::Value());
  debug_value.Set("attempt_count", base::saturated_cast<int>(
                                       generated_icon_fix->attempt_count()));
  return base::Value(std::move(debug_value));
}

bool operator==(const GeneratedIconFix& a, const GeneratedIconFix& b) {
  return a.SerializeAsString() == b.SerializeAsString();
}

}  // namespace web_app
