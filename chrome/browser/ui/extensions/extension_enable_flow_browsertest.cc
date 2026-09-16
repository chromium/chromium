// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_test_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
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

  ~TestManagementProvider() override = default;

  // MananagementPolicy::Provider:
  std::string GetDebugPolicyProviderName() const override { return "test"; }
  bool MustRemainDisabled(
      const extensions::Extension* extension,
      extensions::disable_reason::DisableReason* reason) const override {
    return extension->id() == extension_id_;
  }

 private:
  const extensions::ExtensionId extension_id_;
};

}  // namespace

using ExtensionEnableFlowTest = extensions::ExtensionBrowserTest;

// Test that trying to enable an extension that's blocked by policy fails
// gracefully. See https://crbug.com/41354742.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       TryEnablingPolicyForbiddenExtension) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("extension").Build();
  extension_registrar()->AddExtension(extension);

  {
    extensions::ScopedTestDialogAutoConfirm auto_confirm(
        extensions::ScopedTestDialogAutoConfirm::ACCEPT);

    extensions::ManagementPolicy* management_policy =
        extensions::ExtensionSystem::Get(profile())->management_policy();
    ASSERT_TRUE(management_policy);
    TestManagementProvider test_provider(extension->id());
    management_policy->RegisterProvider(&test_provider);
    extension_registrar()->DisableExtension(
        extension->id(),
        {extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY});
    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(extension->id()));

    ExtensionEnableFlowTestDelegate delegate;

    ExtensionEnableFlow enable_flow(profile(), extension->id(), &delegate);

    content::WebContents* web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    enable_flow.StartForWebContents(web_contents);
    delegate.Wait();

    ASSERT_TRUE(delegate.result());
    EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(extension->id()));

    management_policy->UnregisterProvider(&test_provider);
  }
}

// Test that trying to enable an extension that is disabled due to greylist
// fails and leaves the extension disabled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       GreylistedExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("greylisted-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_GREYLIST});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));
}

// Test that trying to enable an extension that is disabled due to allowlist
// enforcement fails and leaves the extension disabled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       NotAllowlistedExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("not-allowlisted-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_NOT_ALLOWLISTED});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_NOT_ALLOWLISTED));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_NOT_ALLOWLISTED));
}

// Test that trying to enable a corrupted extension fails and leaves the
// extension disabled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       CorruptedExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("corrupted-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_CORRUPTED});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_CORRUPTED));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_CORRUPTED));
}

// Test that trying to enable an extension with unsupported requirements fails
// and leaves the extension disabled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       UnsupportedRequirementExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("unsupported-req-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_UNSUPPORTED_REQUIREMENT));
}

// Test that an extension disabled only by user action can be enabled directly
// without prompting.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       UserActionDisabledExtensionCanBeEnabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("user-action-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_USER_ACTION});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasOnlyDisableReason(
      id, extensions::disable_reason::DISABLE_USER_ACTION));

  // Even if dialogs are set to CANCEL, no dialog should be shown for only
  // user action disable reason.
  extensions::ScopedTestDialogAutoConfirm auto_cancel(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::FINISHED, *delegate.result());

  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->GetDisableReasons(id).empty());
}

// Test that an extension disabled by both user action and greylist cannot be
// enabled and remains disabled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       CombinedUserActionAndGreylistExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("user-action-greylist-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_USER_ACTION,
           extensions::disable_reason::DISABLE_GREYLIST});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_USER_ACTION));

  extensions::ScopedTestDialogAutoConfirm auto_confirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));
}

// Test that an extension disabled for permissions increase shows a prompt and
// can be enabled when the prompt is accepted.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       PermissionsIncreaseExtensionAcceptEnables) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("permissions-increase-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE));

  extensions::ScopedTestDialogAutoConfirm auto_accept(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::FINISHED, *delegate.result());

  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->GetDisableReasons(id).empty());
}

// Test that an extension disabled for permissions increase aborts and remains
// disabled when the prompt is canceled.
IN_PROC_BROWSER_TEST_F(ExtensionEnableFlowTest,
                       PermissionsIncreaseExtensionCancelAborts) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("permissions-increase-cancel-extension")
          .Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE));

  extensions::ScopedTestDialogAutoConfirm auto_cancel(
      extensions::ScopedTestDialogAutoConfirm::CANCEL);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE));
}

// Test that an extension disabled for both permissions increase and greylist
// cannot be enabled even when the prompt is auto-accepted.
IN_PROC_BROWSER_TEST_F(
    ExtensionEnableFlowTest,
    CombinedPermissionsIncreaseAndGreylistExtensionRemainsDisabled) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("permissions-greylist-extension").Build();
  extension_registrar()->AddExtension(extension);
  const extensions::ExtensionId id = extension->id();

  extension_registrar()->DisableExtension(
      id, {extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE,
           extensions::disable_reason::DISABLE_GREYLIST});

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());
  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_PERMISSIONS_INCREASE));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));

  extensions::ScopedTestDialogAutoConfirm auto_accept(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT);

  ExtensionEnableFlowTestDelegate delegate;
  ExtensionEnableFlow enable_flow(profile(), id, &delegate);
  enable_flow.StartForWebContents(
      browser()->GetTabStripModel()->GetActiveWebContents());
  delegate.Wait();

  EXPECT_TRUE(delegate.result().has_value());
  EXPECT_EQ(ExtensionEnableFlowTestDelegate::ABORTED, *delegate.result());

  EXPECT_TRUE(extension_registry()->disabled_extensions().Contains(id));
  EXPECT_FALSE(extension_registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(prefs->HasDisableReason(
      id, extensions::disable_reason::DISABLE_GREYLIST));
}
