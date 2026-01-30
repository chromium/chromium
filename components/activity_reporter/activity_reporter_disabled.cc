// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback.h"
#include "base/version_info/channel.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"

namespace activity_reporter {

// Must be called on a SequencedTaskRunner.
std::unique_ptr<ActivityReporter> CreateActivityReporter(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install) {
  return CreateActivityReporterDisabled();
}

}  // namespace activity_reporter
