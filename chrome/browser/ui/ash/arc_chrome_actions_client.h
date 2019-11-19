// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ARC_CHROME_ACTIONS_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_ARC_CHROME_ACTIONS_CLIENT_H_

#include "components/arc/intent_helper/factory_reset_delegate.h"

namespace user_manager {
class User;
}

// Allows ARC to call into browser code via ArcIntentHelperBridge, such
// as factory resetting ARC itself from Chrome.
// This should only for browser code not related to opening web URLs.
// For that, please use OpenUrlDelegate.
class ArcChromeActionsClient : public arc::FactoryResetDelegate {
 public:
  ArcChromeActionsClient();
  ~ArcChromeActionsClient() override;

  void ResetArc() override;
  // Returns the primary user since it is not possible for
  // non-primary user (e.g. secondary profile) to use ARC.
  static const user_manager::User* GetArcUser();

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcChromeActionsClient);
};

#endif  // CHROME_BROWSER_UI_ASH_ARC_CHROME_ACTIONS_CLIENT_H_
