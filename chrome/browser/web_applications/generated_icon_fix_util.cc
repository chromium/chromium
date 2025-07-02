// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/generated_icon_fix_util.h"

#include <stdint.h>

#include <compare>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/with_app_resources.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/time.h"

namespace web_app::generated_icon_fix_util {

namespace {

constexpr base::TimeDelta kFixWindowDuration = base::Days(7);
constexpr base::TimeDelta kFixAttemptThrottleDuration = base::Days(1);

// Capping the number of attempts should be redundant with the throttle + window
// but, because retries on failure are self propagating, have an explicit
// attempt count to be extra safe. Ordinarily a constraint like this would be
// CHECK'd but because these attempts run at start up it wouldn't be good for
// stability.
constexpr uint32_t kMaxAttemptCount =
    kFixWindowDuration.IntDiv(kFixAttemptThrottleDuration);
static_assert(kMaxAttemptCount == 7u);

std::optional<base::Time> g_now_override_for_testing_;

base::Time Now() {
  return g_now_override_for_testing_.value_or(base::Time::Now());
}

}  // namespace

bool IsValid(const proto::GeneratedIconFix& generated_icon_fix) {
  return generated_icon_fix.has_source() &&
         generated_icon_fix.source() !=
             proto::GENERATED_ICON_FIX_SOURCE_UNKNOWN &&
         generated_icon_fix.has_window_start_time() &&
         generated_icon_fix.has_attempt_count();
}

void SetNowForTesting(base::Time now) {
  g_now_override_for_testing_ = now;
}

bool HasRemainingAttempts(const WebApp& app) {
  const std::optional<proto::GeneratedIconFix>& generated_icon_fix =
      app.generated_icon_fix();
  if (!generated_icon_fix.has_value()) {
    return true;
  }
  return generated_icon_fix->attempt_count() < kMaxAttemptCount;
}

bool IsWithinFixTimeWindow(const WebApp& app) {
  const std::optional<proto::GeneratedIconFix>& generated_icon_fix =
      app.generated_icon_fix();
  if (!generated_icon_fix.has_value()) {
    return true;
  }

  base::TimeDelta duration_since_window_started =
      Now() - syncer::ProtoTimeToTime(generated_icon_fix->window_start_time());
  return duration_since_window_started <= kFixWindowDuration;
}

void EnsureFixTimeWindowStarted(WithAppResources& resources,
                                ScopedRegistryUpdate& update,
                                const webapps::AppId& app_id,
                                proto::GeneratedIconFixSource source) {
  if (resources.registrar()
          .GetAppById(app_id)
          ->generated_icon_fix()
          .has_value()) {
    return;
  }
  update->UpdateApp(app_id)->SetGeneratedIconFix(
      CreateInitialTimeWindow(source));
}

proto::GeneratedIconFix CreateInitialTimeWindow(
    proto::GeneratedIconFixSource source) {
  proto::GeneratedIconFix generated_icon_fix;
  generated_icon_fix.set_source(source);
  generated_icon_fix.set_window_start_time(syncer::TimeToProtoTime(Now()));
  generated_icon_fix.set_attempt_count(0);
  return generated_icon_fix;
}

base::TimeDelta GetThrottleDuration(const WebApp& app) {
  const std::optional<proto::GeneratedIconFix> generated_icon_fix =
      app.generated_icon_fix();
  if (!generated_icon_fix.has_value() ||
      !generated_icon_fix->has_last_attempt_time()) {
    return base::TimeDelta();
  }

  base::TimeDelta throttle_duration =
      syncer::ProtoTimeToTime(generated_icon_fix->last_attempt_time()) +
      kFixAttemptThrottleDuration - Now();
  // Negative durations could cause us to skip ahead of other tasks already in
  // the task queue when used in PostDelayedTask() so clamp to 0.
  return throttle_duration.is_negative() ? base::TimeDelta()
                                         : throttle_duration;
}

void RecordFixAttempt(WithAppResources& resources,
                      ScopedRegistryUpdate& update,
                      const webapps::AppId& app_id,
                      proto::GeneratedIconFixSource source) {
  EnsureFixTimeWindowStarted(resources, update, app_id, source);
  WebApp* app = update->UpdateApp(app_id);
  proto::GeneratedIconFix generated_icon_fix =
      app->generated_icon_fix().value();
  generated_icon_fix.set_attempt_count(generated_icon_fix.attempt_count() + 1);
  generated_icon_fix.set_last_attempt_time(syncer::TimeToProtoTime(Now()));
  app->SetGeneratedIconFix(std::move(generated_icon_fix));
}

}  // namespace web_app::generated_icon_fix_util
