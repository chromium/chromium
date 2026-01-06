// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "components/activity_reporter/activity_reporter.h"

namespace activity_reporter {

// Must be called on a SequencedTaskRunner.
std::unique_ptr<ActivityReporter> CreateActivityReporter() {
  return CreateActivityReporterDisabled();
}

}  // namespace activity_reporter
