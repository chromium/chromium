// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_IMPL_H_
#define COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"

class PrefService;

namespace update_client {
class NetworkFetcherFactory;
}  // namespace update_client

namespace activity_reporter {

class ActivityReporter;

// Creates an ActivityReporter that reports active use to the update server,
// even in builds without `report_activity`. The PrefService that
// `pref_service_provider` returns, if any, must have the prefs from
// `update_client::RegisterPrefs()`. See `CreateActivityReporter()`.
std::unique_ptr<ActivityReporter> CreateActivityReporterImpl(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install);

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_IMPL_H_
