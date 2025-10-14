// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/passkey_unlock_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/passkey_unlock_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "device/fido/features.h"

namespace webauthn {

namespace {

class PasskeyUnlockManagerBrowserTest : public InProcessBrowserTest {
 public:
  PasskeyUnlockManagerBrowserTest() = default;
  ~PasskeyUnlockManagerBrowserTest() override = default;

  webauthn::PasskeyUnlockManager* passkey_unlock_manager() {
    return webauthn::PasskeyUnlockManagerFactory::GetForProfile(
        browser()->profile());
  }

 private:
  base::test::ScopedFeatureList feature_list_{device::kPasskeyUnlockErrorUi};
};

IN_PROC_BROWSER_TEST_F(PasskeyUnlockManagerBrowserTest, IsCreated) {
  EXPECT_NE(passkey_unlock_manager(), nullptr);
}

}  // namespace

}  // namespace webauthn
