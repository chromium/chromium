// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_OMNIBOX_HTTPS_UPGRADE_METRICS_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_OMNIBOX_HTTPS_UPGRADE_METRICS_H_

namespace security_interstitials::omnibox_https_upgrades {

extern const char kEventHistogram[];

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Event {
  kNone = 0,
  // Started the load of an upgraded HTTPS URL.
  kHttpsLoadStarted,
  // Successfully finished loading the upgraded HTTPS URL.
  kHttpsLoadSucceeded,
  // Failed to load the upgraded HTTPS URL because of a cert error, fell back
  // to the HTTP URL.
  kHttpsLoadFailedWithCertError,
  // Failed to load the upgraded HTTPS URL because of a net error, fell back
  // to the HTTP URL.
  kHttpsLoadFailedWithNetError,
  // Failed to load the upgraded HTTPS URL within the timeout window, fell
  // back to the HTTP URL.
  kHttpsLoadTimedOut,
  // Received a redirect. This doesn't necessarily imply that the HTTPS load
  // succeeded or failed.
  kRedirected,
  kMaxValue = kRedirected,
};

}  // namespace security_interstitials::omnibox_https_upgrades

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_OMNIBOX_HTTPS_UPGRADE_METRICS_H_
