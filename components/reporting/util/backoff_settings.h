// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_UTIL_BACKOFF_SETTINGS_H_
#define COMPONENTS_REPORTING_UTIL_BACKOFF_SETTINGS_H_

#include <memory>

#include "net/base/backoff_entry.h"

namespace reporting {

// Returns a BackoffEntry object that defaults to initial 10 second delay and
// doubles the delay on every failure, to a maximum delay of 90 seconds.
// Caller owns the object and is responsible for resetting the delay on
// successful completion.
std::unique_ptr<::net::BackoffEntry> GetBackoffEntry();

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_UTIL_BACKOFF_SETTINGS_H_
