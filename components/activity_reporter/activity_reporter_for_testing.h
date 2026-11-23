// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_
#define COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"

class PrefService;

namespace base {
class Clock;
}  // namespace base

namespace update_client {
class UpdateClient;
}  // namespace update_client

namespace activity_reporter {

class ActivityReporter;

// Creates an ActivityReporter that sends its update checks to `update_client`
// and reads the time from `clock`. `clock` must be non-null and outlive the
// reporter. The PrefService that `pref_service_provider` returns, if any, must
// have the prefs from `update_client::RegisterPrefs()`. See
// `CreateActivityReporter()`.
std::unique_ptr<ActivityReporter> CreateActivityReporterForTesting(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::UpdateClient> update_client,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    const base::Clock* clock);

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_
