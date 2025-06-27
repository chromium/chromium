// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context.h"

#include "chrome/browser/password_manager/password_manager_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webid/federated_identity_auto_reauthn_permission_context_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

class FederatedIdentityAutoReauthnPermissionContextTest
    : public InProcessBrowserTest {
 public:
  FederatedIdentityAutoReauthnPermissionContextTest() = default;

  FederatedIdentityAutoReauthnPermissionContextTest(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;
  FederatedIdentityAutoReauthnPermissionContextTest& operator=(
      const FederatedIdentityAutoReauthnPermissionContextTest&) = delete;

  ~FederatedIdentityAutoReauthnPermissionContextTest() override = default;
};

// Tests PasswordManagerSettingsService correctly hooks itself as a cyclic
// dependency. Regression test for crbug.com/428112191.
IN_PROC_BROWSER_TEST_F(FederatedIdentityAutoReauthnPermissionContextTest,
                       AutoReauthnSettingEnabledByDefault) {
  // Force PasswordManagerSettingsService instantiation.
  PasswordManagerSettingsServiceFactory::GetForProfile(browser()->profile());

  EXPECT_TRUE(
      FederatedIdentityAutoReauthnPermissionContextFactory::GetForProfile(
          browser()->profile())
          ->IsAutoReauthnSettingEnabled());
}

}  // namespace
