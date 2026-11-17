// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/activity_reporter_impl.h"
#include "components/update_client/network.h"

namespace activity_reporter {

std::unique_ptr<ActivityReporter> CreateActivityReporter(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install) {
  return CreateActivityReporterImpl(
      std::move(pref_service_provider), std::move(network_fetcher_factory),
      std::move(channel_provider), std::move(brand_provider),
      std::move(updater_active_callback), per_user_install);
}

}  // namespace activity_reporter
