// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/ntp4_browsertest.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"

NTP4LoggedInWebUITest::NTP4LoggedInWebUITest() {}

NTP4LoggedInWebUITest::~NTP4LoggedInWebUITest() {}

void NTP4LoggedInWebUITest::SetLoginName(const std::string& name) {
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  signin::SetPrimaryAccount(identity_manager, name);
}
