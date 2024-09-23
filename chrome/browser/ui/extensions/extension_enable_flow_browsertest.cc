// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_test_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"

namespace {

class TestManagementProvider : public extensions::ManagementPolicy::Provider {
 public:
  explicit TestManagementProvider(const extensions::ExtensionId& extension_id)
      : extension_id_(extension_id) {}

  TestManagementProvider(const TestManagementProvider&) = delete;
  TestManagementProvider& operator=(const TestManagementProvider&) = delete;

  ~TestManagementProvider() override {}

  // MananagementPolicy::Provider:
  std::string GetDebugPolicyProviderName() const override { return "test"; }
  bool MustRemainDisabled(const extensions::Extension* extension,
                          extensions::disable_reason::DisableReason* reason,
                          std::u16string* error) const override {
    return extension->id() == extension_id_;
  }

 private:
  const extensions::ExtensionId extension_id_;
};

}  // namespace

using ExtensionEnableFlowTest = extensions::ExtensionBrowserTest;

// Test that trying to enable an extension that's blocked by policy fails
// gracefully. See https://crbug.com/783831.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       TryEnablingPolicyForbiddenExtension) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension").Build();
  extension_service()->AddExtension(extension.get());

  {
    extensions::ScopedTestDialogAutoConfirm auto_confirm(
        extensions::ScopedTestDialogAutoConfirm::ACCEPT);

    extensions::ManagementPolicy* management_policy =
        extensions::ExtensionSystem::Get(profile())->management_policy();
    ASSERT_TRUE(management_policy);
    TestManagementProvider test_provider(extension->id());
    management_policy->RegisterProvider(&test_provider);
    extension_service()->DisableExtension(
        extension->id(), extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY);
    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(extension->id()));

    ExtensionEnableFlowTestDelegate delegate;

    ExtensionEnableFlow enable_flow(profile(), extension->id(), &delegate);

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    enable_flow.StartForWebContents(web_contents);
    delegate.Wait();

    ASSERT_TRUE(delegate.result());
    EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(extension->id()));

    management_policy->UnregisterProvider(&test_provider);
  }
}
