// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

namespace messages {

const base::Feature kMessagesForAndroidInfrastructure{
    "MessagesForAndroidInfrastructure", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidPasswords{
    "MessagesForAndroidPasswords", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kMessagesForAndroidPopupBlocked{
    "MessagesForAndroidPopupBlocked", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kMessagesForAndroidSafetyTip{
    "MessagesForAndroidSafetyTip", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kMessagesForAndroidSaveCard{
    "MessagesForAndroidSaveCard", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidUpdatePassword{
    "MessagesForAndroidUpdatePassword", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsPasswordMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPasswords);
}

bool IsPopupBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPopupBlocked);
}

bool IsSafetyTipMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidSafetyTip);
}

bool IsSaveCardMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidSaveCard);
}

bool IsUpdatePasswordMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidUpdatePassword);
}

}  // namespace messages
