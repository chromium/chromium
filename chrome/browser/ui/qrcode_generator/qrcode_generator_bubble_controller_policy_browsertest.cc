// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace qrcode_generator {

// Tests that verify that the `QRCodeGeneratorBubbleController` respects the
// policy that controls whether the QR code generator is enabled.
class QRCodeGeneratorBubbleControllerPolicyTest : public policy::PolicyTest {
 public:
  QRCodeGeneratorBubbleControllerPolicyTest() = default;

  QRCodeGeneratorBubbleControllerPolicyTest(
      const QRCodeGeneratorBubbleControllerPolicyTest&) = delete;
  QRCodeGeneratorBubbleControllerPolicyTest& operator=(
      const QRCodeGeneratorBubbleControllerPolicyTest&) = delete;

  ~QRCodeGeneratorBubbleControllerPolicyTest() override = default;

  QRCodeGeneratorBubbleController* controller() { return controller_; }

  void SetEnabledByPolicy(const bool enabled) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kQRCodeGeneratorEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM, base::Value(enabled), nullptr);
    UpdateProviderPolicy(policies);

    PrefService* prefs = g_browser_process->local_state();
    ASSERT_EQ(enabled, prefs->GetBoolean(prefs::kQRCodeGeneratorEnabled));
  }

 protected:
  void SetUpOnMainThread() override {
    policy::PolicyTest::SetUpOnMainThread();
    controller_ = QRCodeGeneratorBubbleController::Get(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(controller_);
  }

  void TearDownOnMainThread() override {
    controller_ = nullptr;
    policy::PolicyTest::TearDownOnMainThread();
  }

 private:
  raw_ptr<QRCodeGeneratorBubbleController> controller_ = nullptr;
};

// Verifies that when the policy is not set, the default policy is used
// (allowed).
IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleControllerPolicyTest, Default) {
  const GURL url("https://www.example.com");

  // Verify the policy defaults to true.
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs->GetBoolean(prefs::kQRCodeGeneratorEnabled));

  ASSERT_TRUE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(url));

  // Show the bubble.
  controller()->ShowBubble(url);
  ASSERT_TRUE(controller()->IsBubbleShown());
}

// Verifies that when the policy is set to false, `IsGeneratorAvailable()`
// returns false even for a valid url, and that attempting to show
// the bubble fails.
IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleControllerPolicyTest, Disabled) {
  const GURL url("https://www.example.com");

  SetEnabledByPolicy(/*enabled=*/false);
  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(url));

  // Try to show the bubble and verify it fails.
  controller()->ShowBubble(url);
  ASSERT_FALSE(controller()->IsBubbleShown());
}

// Verifies that when the policy is set to true, `IsGeneratorAvailable()`
// returns true for a valid url, and that the bubble can be shown.
IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleControllerPolicyTest, Enabled) {
  const GURL url("https://www.example.com");

  SetEnabledByPolicy(/*enabled=*/true);
  ASSERT_TRUE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(url));

  // Show the bubble.
  controller()->ShowBubble(url);
  ASSERT_TRUE(controller()->IsBubbleShown());
}

// Verifies that when the policy is set to false, an open QR Code
// Generator is closed (i.e. tests that the policy's dynamic_refresh is
// honored).
IN_PROC_BROWSER_TEST_F(QRCodeGeneratorBubbleControllerPolicyTest,
                       DynamicRefresh) {
  const GURL url("https://www.example.com");

  // Show the bubble.
  controller()->ShowBubble(url);
  ASSERT_TRUE(controller()->IsBubbleShown());

  // Disable the policy.
  SetEnabledByPolicy(/*enabled=*/false);

  // Verify the bubble was closed.
  ASSERT_FALSE(controller()->IsBubbleShown());

  ASSERT_FALSE(QRCodeGeneratorBubbleController::IsGeneratorAvailable(url));
}

}  // namespace qrcode_generator
