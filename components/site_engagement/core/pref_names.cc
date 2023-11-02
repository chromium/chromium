// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_engagement/core/pref_names.h"

namespace site_engagement {
namespace prefs {

// The last time that the site engagement service recorded an engagement event
// for this profile for any URL. Recorded only during shutdown. Used to prevent
// the service from decaying engagement when a user does not use the browser at
// all for an extended period of time.
const char kSiteEngagementLastUpdateTime[] = "profile.last_engagement_time";

}  // namespace prefs
}  // namespace site_engagement
