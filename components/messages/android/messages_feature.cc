// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

namespace messages {

const base::Feature kMessagesForAndroidInfrastructure{
    "MessagesForAndroidInfrastructure", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidPasswords{
    "MessagesForAndroidPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPasswordMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPasswords);
}

}  // namespace messages
