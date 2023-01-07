// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_CONSTANTS_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_CONSTANTS_H_

#include "base/files/file_path.h"

namespace ntp_snippets {

// Name of the folder where the snippets database should be stored. This is only
// the name of the folder, not a full path - it must be appended to e.g. the
// profile path.
extern const base::FilePath::CharType kDatabaseFolder[];

// OAuth access token scope.
extern const char kContentSuggestionsApiScope[];

// Server endpoint for fetching snippets.
extern const char kContentSuggestionsServer[];

// Server endpoints for push updates subscription.
extern const char kPushUpdatesSubscriptionServer[];  // used on stable/beta
extern const char
    kPushUpdatesSubscriptionStagingServer[];              // used on dev/canary
extern const char kPushUpdatesSubscriptionAlphaServer[];  // for testing

// Server endpoints for push updates unsubscription.
extern const char kPushUpdatesUnsubscriptionServer[];  // used on stable/beta
extern const char
    kPushUpdatesUnsubscriptionStagingServer[];  // used on dev/canary
extern const char kPushUpdatesUnsubscriptionAlphaServer[];  // for testing

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_CONSTANTS_H_
