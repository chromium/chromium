// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/controlled_home_dialog_controller.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

class ControlledHomeDialogControllerTest
    : public extensions::ExtensionBrowserTest {
 public:
  ControlledHomeDialogControllerTest() = default;
  ~ControlledHomeDialogControllerTest() override = default;

  // Loads an extension that overrides the home page of a user.
  scoped_refptr<const extensions::Extension> LoadExtensionOverridingHome(
      const std::string& name = "extension",
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto temp_dir = std::make_unique<base::ScopedTempDir>();
    EXPECT_TRUE(temp_dir->CreateUniqueTempDir());

    base::DictValue manifest;
    manifest.Set("name", name);
    manifest.Set("version", "1.0");
    manifest.Set("manifest_version", 3);

    base::DictValue overrides;
    overrides.Set("homepage", "http://www.google.com");
    manifest.Set("chrome_settings_overrides", std::move(overrides));

    base::FilePath manifest_path =
        temp_dir->GetPath().AppendASCII("manifest.json");
    EXPECT_TRUE(base::WriteFile(manifest_path, *base::WriteJson(manifest)));

    extensions::ChromeTestExtensionLoader loader(profile());
    loader.set_location(location);
    if (location != extensions::mojom::ManifestLocation::kInternal) {
      loader.set_pack_extension(true);
    }
    loader.set_grant_permissions(true);

    scoped_refptr<const extensions::Extension> extension =
        loader.LoadExtension(temp_dir->GetPath());
    EXPECT_TRUE(extension);

    temp_dirs_.push_back(std::move(temp_dir));
    return extension;
  }

  // Returns true if the extension is enabled.
  bool IsExtensionEnabled(const extensions::ExtensionId& id) {
    return extension_registry()->enabled_extensions().GetByID(id);
  }

  // Returns true if the extension is disabled and has the specified
  // `disable_reason`.
  bool IsExtensionDisabled(
      const extensions::ExtensionId& id,
      extensions::disable_reason::DisableReason disable_reason) {
    return extension_registry()->disabled_extensions().GetByID(id) &&
           extension_prefs_->HasOnlyDisableReason(id, disable_reason);
  }

  // Returns true if the extension has been acknowledged by the user.
  bool IsExtensionAcknowledged(const extensions::ExtensionId& id) {
    bool was_acknowledged = false;
    return extension_prefs_->ReadPrefAsBoolean(
               id, ControlledHomeDialogController::kAcknowledgedPreference,
               &was_acknowledged) &&
           was_acknowledged;
  }

  // Acknowledges the extension in preferences.
  void AcknowledgeExtension(const extensions::ExtensionId& id) {
    extension_prefs_->UpdateExtensionPref(
        id, ControlledHomeDialogController::kAcknowledgedPreference,
        base::Value(true));
  }

 private:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    extension_prefs_ = extensions::ExtensionPrefs::Get(profile());
  }

  void TearDownOnMainThread() override {
    extension_prefs_ = nullptr;
    WaitForStorageCleanup();
    ControlledHomeDialogController::ClearProfileSetForTesting();
    temp_dirs_.clear();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void WaitForStorageCleanup() {
    content::StoragePartition* partition =
        profile()->GetDefaultStoragePartition();
    if (partition) {
      partition->WaitForDeletionTasksForTesting();
    }
  }

  base::AutoReset<bool> ignore_learn_more_{
      ControlledHomeDialogController::IgnoreLearnMoreForTesting()};
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  std::unique_ptr<base::CommandLine> command_line_;
  std::vector<std::unique_ptr<base::ScopedTempDir>> temp_dirs_;
};

// Though the test harness should compile on all platforms, the behavior for
// extensions to override the home page is limited to mac and windows.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       ClickingExecuteDisablesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  EXPECT_TRUE(dialog_controller->ShouldShow());
  EXPECT_EQ(extension, dialog_controller->extension_for_testing());

  dialog_controller->PendingShow();
  dialog_controller->OnBubbleShown();

  dialog_controller->OnBubbleClosed(
      ControlledHomeDialogControllerInterface::CLOSE_EXECUTE);

  EXPECT_TRUE(IsExtensionDisabled(
      extension->id(), extensions::disable_reason::DISABLE_USER_ACTION));
  // Since the extension was disabled, it shouldn't be acknowledged in
  // preferences.
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       ClickingDismissAcknowledgesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  EXPECT_TRUE(dialog_controller->ShouldShow());
  EXPECT_EQ(extension, dialog_controller->extension_for_testing());

  dialog_controller->PendingShow();
  dialog_controller->OnBubbleShown();

  dialog_controller->OnBubbleClosed(
      ControlledHomeDialogControllerInterface::CLOSE_DISMISS_USER_ACTION);

  // The extension should remain enabled and be acknowledged.
  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_TRUE(IsExtensionAcknowledged(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       DismissByDeactivationDoesNotDisableOrAcknowledge) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    EXPECT_TRUE(dialog_controller->ShouldShow());
    EXPECT_EQ(extension, dialog_controller->extension_for_testing());

    dialog_controller->PendingShow();
    dialog_controller->OnBubbleShown();

    dialog_controller->OnBubbleClosed(
        ControlledHomeDialogControllerInterface::CLOSE_DISMISS_DEACTIVATION);
  }

  // The extension should remain enabled but *shouldn't* be acknowledged.
  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    // Even though the extension hasn't been acknowledged, we shouldn't show the
    // bubble twice in the same session.
    EXPECT_FALSE(dialog_controller->ShouldShow());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       ClickingLearnMoreAcknowledgesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  EXPECT_TRUE(dialog_controller->ShouldShow());
  EXPECT_EQ(extension, dialog_controller->extension_for_testing());

  dialog_controller->PendingShow();
  dialog_controller->OnBubbleShown();

  dialog_controller->OnBubbleClosed(
      ControlledHomeDialogControllerInterface::CLOSE_LEARN_MORE);

  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_TRUE(IsExtensionAcknowledged(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       BubbleShouldntShowIfExtensionAcknowledged) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  AcknowledgeExtension(extension->id());

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  EXPECT_FALSE(dialog_controller->ShouldShow());
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       LongExtensionNameIsTruncated) {
  const std::u16string long_name =
      u"This extension name should be longer than our truncation threshold "
      "to test that the bubble can handle long names";
  const std::u16string truncated_name =
      extensions::ui_util::GetFixupExtensionNameForUIDisplay(long_name);
  ASSERT_LT(truncated_name.size(), long_name.size());

  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome(base::UTF16ToUTF8(long_name));
  ASSERT_TRUE(extension);

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  EXPECT_TRUE(dialog_controller->ShouldShow());

  std::u16string bubble_text = dialog_controller->GetBodyText();

  EXPECT_FALSE(bubble_text.contains(long_name));
  EXPECT_TRUE(bubble_text.contains(truncated_name));
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       ExecutingOnOneExtensionDoesntAffectAnotherExtension) {
  scoped_refptr<const extensions::Extension> extension1 =
      LoadExtensionOverridingHome("ext1");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtensionOverridingHome("ext2");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    EXPECT_TRUE(dialog_controller->ShouldShow());
    // The most-recently-installed extension should control the home page
    // (`extension2`).
    EXPECT_EQ(extension2, dialog_controller->extension_for_testing());

    dialog_controller->PendingShow();
    dialog_controller->OnBubbleShown();

    // Close the bubble with the "execute" action.
    dialog_controller->OnBubbleClosed(
        ControlledHomeDialogControllerInterface::CLOSE_EXECUTE);

    EXPECT_TRUE(IsExtensionDisabled(
        extension2->id(), extensions::disable_reason::DISABLE_USER_ACTION));
    EXPECT_TRUE(IsExtensionEnabled(extension1->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension2->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension1->id()));
  }

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    // Since `extension2` was removed, we shouldn't have acknowledged either
    // extension and we can re-show the bubble if the homepage is controlled
    // by another extension.
    EXPECT_TRUE(dialog_controller->ShouldShow());
    EXPECT_EQ(extension1, dialog_controller->extension_for_testing());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       AcknowledgingOneExtensionDoesntAffectAnother) {
  scoped_refptr<const extensions::Extension> extension1 =
      LoadExtensionOverridingHome("ext1");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtensionOverridingHome("ext2");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    EXPECT_TRUE(dialog_controller->ShouldShow());
    EXPECT_EQ(extension2, dialog_controller->extension_for_testing());

    dialog_controller->PendingShow();
    dialog_controller->OnBubbleShown();

    // Dismiss the bubble; this acknowledges the extension.
    dialog_controller->OnBubbleClosed(
        ControlledHomeDialogControllerInterface::CLOSE_DISMISS_USER_ACTION);

    EXPECT_TRUE(IsExtensionEnabled(extension2->id()));
    EXPECT_TRUE(IsExtensionAcknowledged(extension2->id()));

    EXPECT_TRUE(IsExtensionEnabled(extension1->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension1->id()));
  }

  {
    // The bubble shouldn't want to show (the extension that controls the home
    // page was acknowledged).
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    EXPECT_FALSE(dialog_controller->ShouldShow());
  }

  // Disable the extension that was acknowledged.
  extension_registrar()->DisableExtension(
      extension2->id(), {extensions::disable_reason::DISABLE_USER_ACTION});

  {
    auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
        profile(), GetActiveWebContents());
    // Now a new extension controls the home page, so we should re-show the
    // bubble.
    EXPECT_TRUE(dialog_controller->ShouldShow());
    EXPECT_EQ(extension1, dialog_controller->extension_for_testing());
  }
}

IN_PROC_BROWSER_TEST_F(ControlledHomeDialogControllerTest,
                       PolicyExtensionsRequirePolicyIndicators) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome(
          "ext", extensions::mojom::ManifestLocation::kExternalPolicy);
  ASSERT_TRUE(extension);

  auto dialog_controller = std::make_unique<ControlledHomeDialogController>(
      profile(), GetActiveWebContents());
  // We still show the bubble for policy-installed extensions, but it should
  // have a policy decoration.
  EXPECT_TRUE(dialog_controller->ShouldShow());

  EXPECT_EQ(u"", dialog_controller->GetActionButtonText());
  EXPECT_TRUE(dialog_controller->IsPolicyIndicationNeeded());
}
#endif
