// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_
#define COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Feature flag to disallow creation of push messages with GCM Sender IDs.
BASE_DECLARE_FEATURE(kPushMessagingDisallowSenderIDs);

// Feature flag to enable push subscription with expiration times specified in
// /chrome/browser/push_messaging/push_messaging_constants.h
BASE_DECLARE_FEATURE(kPushSubscriptionWithExpirationTime);

// Feature flag to control use of new /wp/ path based Webpush endpoints.
BASE_DECLARE_FEATURE(kPushMessagingGcmEndpointWebpushPath);

}  // namespace features

#endif  // COMPONENTS_PUSH_MESSAGING_PUSH_MESSAGING_FEATURES_H_
