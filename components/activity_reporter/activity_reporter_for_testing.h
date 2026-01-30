// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_
#define COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"

namespace update_client {
class UpdateClient;
}  // namespace update_client

namespace activity_reporter {

class ActivityReporter;

std::unique_ptr<ActivityReporter> CreateActivityReporterForTesting(
    scoped_refptr<update_client::UpdateClient> update_client,
    base::RepeatingClosure updater_active_callback,
    base::RepeatingCallback<version_info::Channel()> channel_provider);

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_ACTIVITY_REPORTER_FOR_TESTING_H_
