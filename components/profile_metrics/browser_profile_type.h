// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROFILE_METRICS_BROWSER_PROFILE_TYPE_H_
#define COMPONENTS_PROFILE_METRICS_BROWSER_PROFILE_TYPE_H_

namespace base {
class SupportsUserData;
}

namespace profile_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.profile_metrics
enum class BrowserProfileType {
  kRegular = 0,
  kIncognito = 1,
  kGuest = 2,
  kSystem = 3,
  kOtherOffTheRecordProfile = 4,
  // Deprecated(10/23): kDeprecatedEphemeralGuest = 5,
  kMaxValue = kOtherOffTheRecordProfile,
};

// Returns the BrowserProfileType value associated with |browser_context|.
// Note that the browser profile type should be set for all BrowserContext (or
// equivalent) objects during creation or initialization of the object. This
// function will result in a crash if |SetBrowserProfileType| is not called
// before to associate the browser profile type.
BrowserProfileType GetBrowserProfileType(
    const base::SupportsUserData* browser_context);

// Associates |type| as the BrowserProfileType value for |browser_context|.
void SetBrowserProfileType(base::SupportsUserData* browser_context,
                           BrowserProfileType type);
}  // namespace profile_metrics

#endif  // COMPONENTS_PROFILE_METRICS_BROWSER_PROFILE_TYPE_H_
