// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_H_
#define COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"

class PrefService;

namespace update_client {
class NetworkFetcherFactory;
}

namespace activity_reporter {

// An ActivityReporter is capable of reporting that Chrome was actively used by
// the user in a privacy-preserving manner.
// Refer to //components/activity_reporter/README.md.
class ActivityReporter {
 public:
  ActivityReporter(const ActivityReporter&) = delete;
  ActivityReporter& operator=(const ActivityReporter&&) = delete;

  // Must be destroyed on the same sequence it was created on.
  virtual ~ActivityReporter() = default;

  // Report that the browser was actively used. Must be called on the same
  // sequence the ActivityReporter was created on. The actual report may take
  // place in a background task. Does not block.
  virtual void ReportActive() = 0;

 protected:
  // Must be called on a SequencedTaskRunner.
  ActivityReporter() = default;
};

// Must be called on a SequencedTaskRunner. When `ReportActive` is called, the
// ActivityReporter will also run `updater_active_callback` if
// USE_LEGACY_ACTIVE_DEFINITION is false.
std::unique_ptr<ActivityReporter> CreateActivityReporter(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install);

// Must be called on a SequencedTaskRunner. Creates an ActivityReporter that
// does nothing.
std::unique_ptr<ActivityReporter> CreateActivityReporterDisabled();

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_H_
