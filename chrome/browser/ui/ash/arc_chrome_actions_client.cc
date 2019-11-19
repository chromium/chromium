// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/arc_chrome_actions_client.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/user_manager/user_manager.h"

ArcChromeActionsClient::ArcChromeActionsClient() {
  arc::ArcIntentHelperBridge::SetFactoryResetDelegate(this);
}

ArcChromeActionsClient::~ArcChromeActionsClient() {
  arc::ArcIntentHelperBridge::SetFactoryResetDelegate(nullptr);
}

void ArcChromeActionsClient::ResetArc() {
  const user_manager::User* user = ArcChromeActionsClient::GetArcUser();
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  arc::SetArcPlayStoreEnabledForProfile(profile, /*enabled=*/false);
  arc::SetArcPlayStoreEnabledForProfile(profile, /*enabled=*/true);
}

// ARC is only running for the primary user.
const user_manager::User* ArcChromeActionsClient::GetArcUser() {
  return user_manager::UserManager::Get()->GetPrimaryUser();
}
