// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/controlled_home_bubble_delegate.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<extensions::ExtensionWebUIOverrideRegistrar>(context);
}

}  // namespace

class ControlledHomeBubbleDelegateTest : public BrowserWithTestWindowTest {
 public:
  ControlledHomeBubbleDelegateTest() = default;
  ~ControlledHomeBubbleDelegateTest() override = default;

  // Loads an extension that overrides the home page of a user.
  scoped_refptr<const extensions::Extension> LoadExtensionOverridingHome(
      const std::string& name = "extension",
      extensions::mojom::ManifestLocation location =
          extensions::mojom::ManifestLocation::kInternal) {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(name)
            .SetManifestVersion(3)
            .SetLocation(location)
            .SetManifestKey(
                "chrome_settings_overrides",
                base::Value::Dict().Set("homepage", "http://www.google.com"))
            .Build();
    extension_service_->GrantPermissions(extension.get());
    extension_service_->AddExtension(extension.get());

    return extension;
  }

  // Returns true if the extension is enabled.
  bool IsExtensionEnabled(const extensions::ExtensionId& id) {
    return extension_registry_->enabled_extensions().GetByID(id);
  }

  // Returns true if the extension is disabled and has the specified
  // `disable_reason`.
  bool IsExtensionDisabled(
      const extensions::ExtensionId& id,
      extensions::disable_reason::DisableReason disable_reason) {
    return extension_registry_->disabled_extensions().GetByID(id) &&
           extension_prefs_->GetDisableReasons(id) == disable_reason;
  }

  // Returns true if the extension has been acknowledged by the user.
  bool IsExtensionAcknowledged(const extensions::ExtensionId& id) {
    bool was_acknowledged = false;
    return extension_prefs_->ReadPrefAsBoolean(
               id, ControlledHomeBubbleDelegate::kAcknowledgedPreference,
               &was_acknowledged) &&
           was_acknowledged;
  }

  // Acknowledges the extension in preferences.
  void AcknowledgeExtension(const extensions::ExtensionId& id) {
    extension_prefs_->UpdateExtensionPref(
        id, ControlledHomeBubbleDelegate::kAcknowledgedPreference,
        base::Value(true));
  }

  extensions::ExtensionService* extension_service() {
    return extension_service_.get();
  }

 private:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Prevent the Profile from getting deleted before TearDown() is complete,
    // since WaitForStorageCleanup() relies on an active Profile. See the
    // DestroyProfileOnBrowserClose flag.
    profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
        profile(), ProfileKeepAliveOrigin::kBrowserWindow);

    // The two lines of magical incantation required to get the extension
    // service to work inside a unit test and access the extension prefs.
    static_cast<extensions::TestExtensionSystem*>(
        extensions::ExtensionSystem::Get(profile()))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);

    // Set up the rest of the necessary systems.
    extension_service_ =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    extension_service_->Init();

    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactory(profile(),
                            base::BindRepeating(&BuildOverrideRegistrar));
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
        profile());

    extension_prefs_ = extensions::ExtensionPrefs::Get(profile());
    extension_registry_ = extensions::ExtensionRegistry::Get(profile());
  }

  void TearDown() override {
    extension_service_ = nullptr;
    extension_prefs_ = nullptr;
    extension_registry_ = nullptr;
    WaitForStorageCleanup();
    // Clean up global state for the delegates. Since profiles are stored in
    // global variables, they can be shared between tests and cause
    // unpredictable behavior.
    ControlledHomeBubbleDelegate::ClearProfileSetForTesting();
    profile_keep_alive_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void WaitForStorageCleanup() {
    content::StoragePartition* partition =
        profile()->GetDefaultStoragePartition();
    if (partition) {
      partition->WaitForDeletionTasksForTesting();
    }
  }

  base::AutoReset<bool> ignore_learn_more_{
      ControlledHomeBubbleDelegate::IgnoreLearnMoreForTesting()};
  raw_ptr<extensions::ExtensionService> extension_service_;
  raw_ptr<extensions::ExtensionPrefs> extension_prefs_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_;
  std::unique_ptr<base::CommandLine> command_line_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

// Though the test harness should compile on all platforms, the behavior for
// extensions to override the home page is limited to mac and windows.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
TEST_F(ControlledHomeBubbleDelegateTest, ClickingExecuteDisablesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  ASSERT_TRUE(browser());
  ASSERT_TRUE(profile());

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  EXPECT_TRUE(bubble_delegate->ShouldShow());
  EXPECT_EQ(extension, bubble_delegate->extension_for_testing());

  bool did_close_programmatically = false;
  auto close_callback = base::BindLambdaForTesting(
      [&did_close_programmatically]() { did_close_programmatically = true; });

  bubble_delegate->PendingShow();
  bubble_delegate->OnBubbleShown(std::move(close_callback));

  EXPECT_FALSE(did_close_programmatically);
  bubble_delegate->OnBubbleClosed(
      ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE);

  EXPECT_TRUE(IsExtensionDisabled(
      extension->id(), extensions::disable_reason::DISABLE_USER_ACTION));
  // Since the extension was disabled, it shouldn't be acknowledged in
  // preferences.
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ControlledHomeBubbleDelegateTest,
       ClickingDismissAcknowledgesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  EXPECT_TRUE(bubble_delegate->ShouldShow());
  EXPECT_EQ(extension, bubble_delegate->extension_for_testing());

  bool did_close_programmatically = false;
  auto close_callback = base::BindLambdaForTesting(
      [&did_close_programmatically]() { did_close_programmatically = true; });

  bubble_delegate->PendingShow();
  bubble_delegate->OnBubbleShown(std::move(close_callback));

  EXPECT_FALSE(did_close_programmatically);
  bubble_delegate->OnBubbleClosed(
      ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION);

  // The extension should remain enabled and be acknowledged.
  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_TRUE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ControlledHomeBubbleDelegateTest,
       DismissByDeactivationDoesNotDisableOrAcknowledge) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    EXPECT_TRUE(bubble_delegate->ShouldShow());
    EXPECT_EQ(extension, bubble_delegate->extension_for_testing());

    bool did_close_programmatically = false;
    auto close_callback = base::BindLambdaForTesting(
        [&did_close_programmatically]() { did_close_programmatically = true; });

    bubble_delegate->PendingShow();
    bubble_delegate->OnBubbleShown(std::move(close_callback));

    EXPECT_FALSE(did_close_programmatically);
    bubble_delegate->OnBubbleClosed(
        ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_DEACTIVATION);
  }

  // The extension should remain enabled but *shouldn't* be acknowledged.
  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    // Even though the extension hasn't been acknowledged, we shouldn't show the
    // bubble twice in the same session.
    EXPECT_FALSE(bubble_delegate->ShouldShow());
  }
}

TEST_F(ControlledHomeBubbleDelegateTest,
       ClickingLearnMoreAcknowledgesTheExtension) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  EXPECT_TRUE(bubble_delegate->ShouldShow());
  EXPECT_EQ(extension, bubble_delegate->extension_for_testing());

  bool did_close_programmatically = false;
  auto close_callback = base::BindLambdaForTesting(
      [&did_close_programmatically]() { did_close_programmatically = true; });

  bubble_delegate->PendingShow();
  bubble_delegate->OnBubbleShown(std::move(close_callback));

  EXPECT_FALSE(did_close_programmatically);
  bubble_delegate->OnBubbleClosed(
      ToolbarActionsBarBubbleDelegate::CLOSE_LEARN_MORE);

  EXPECT_TRUE(IsExtensionEnabled(extension->id()));
  EXPECT_TRUE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ControlledHomeBubbleDelegateTest, DisablingTheExtensionClosesTheBubble) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  EXPECT_TRUE(bubble_delegate->ShouldShow());
  EXPECT_EQ(extension, bubble_delegate->extension_for_testing());

  bool did_close_programmatically = false;
  auto close_callback = base::BindLambdaForTesting(
      [&did_close_programmatically]() { did_close_programmatically = true; });

  bubble_delegate->PendingShow();
  bubble_delegate->OnBubbleShown(std::move(close_callback));

  extension_service()->DisableExtension(
      extension->id(), extensions::disable_reason::DISABLE_USER_ACTION);

  // The bubble should close as part of the extension being unloaded.
  EXPECT_TRUE(did_close_programmatically);
  // And it should remain unacknowledged.
  EXPECT_FALSE(IsExtensionAcknowledged(extension->id()));
}

TEST_F(ControlledHomeBubbleDelegateTest,
       BubbleShouldntShowIfExtensionAcknowledged) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome();
  ASSERT_TRUE(extension);

  AcknowledgeExtension(extension->id());

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  EXPECT_FALSE(bubble_delegate->ShouldShow());
}

TEST_F(ControlledHomeBubbleDelegateTest,
       ExecutingOnOneExtensionDoesntAffectAnotherExtension) {
  scoped_refptr<const extensions::Extension> extension1 =
      LoadExtensionOverridingHome("ext1");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtensionOverridingHome("ext2");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    EXPECT_TRUE(bubble_delegate->ShouldShow());
    // The most-recently-installed extension should control the home page
    // (`extension2`).
    EXPECT_EQ(extension2, bubble_delegate->extension_for_testing());

    bool did_close_programmatically = false;
    auto close_callback = base::BindLambdaForTesting(
        [&did_close_programmatically]() { did_close_programmatically = true; });

    bubble_delegate->PendingShow();
    bubble_delegate->OnBubbleShown(std::move(close_callback));

    // Close the bubble with the "execute" action.
    bubble_delegate->OnBubbleClosed(
        ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE);

    EXPECT_TRUE(IsExtensionDisabled(
        extension2->id(), extensions::disable_reason::DISABLE_USER_ACTION));
    EXPECT_TRUE(IsExtensionEnabled(extension1->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension2->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension1->id()));
  }

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    // Since `extension2` was removed, we shouldn't have acknowledged either
    // extension and we can re-show the bubble if the homepage is controlled
    // by another extension.
    EXPECT_TRUE(bubble_delegate->ShouldShow());
    EXPECT_EQ(extension1, bubble_delegate->extension_for_testing());
  }
}

TEST_F(ControlledHomeBubbleDelegateTest,
       AcknowledgingOneExtensionDoesntAffectAnother) {
  scoped_refptr<const extensions::Extension> extension1 =
      LoadExtensionOverridingHome("ext1");
  scoped_refptr<const extensions::Extension> extension2 =
      LoadExtensionOverridingHome("ext2");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    EXPECT_TRUE(bubble_delegate->ShouldShow());
    EXPECT_EQ(extension2, bubble_delegate->extension_for_testing());

    bool did_close_programmatically = false;
    auto close_callback = base::BindLambdaForTesting(
        [&did_close_programmatically]() { did_close_programmatically = true; });

    bubble_delegate->PendingShow();
    bubble_delegate->OnBubbleShown(std::move(close_callback));

    // Dismiss the bubble; this acknowledges the extension.
    bubble_delegate->OnBubbleClosed(
        ToolbarActionsBarBubbleDelegate::CLOSE_DISMISS_USER_ACTION);

    EXPECT_TRUE(IsExtensionEnabled(extension2->id()));
    EXPECT_TRUE(IsExtensionAcknowledged(extension2->id()));

    EXPECT_TRUE(IsExtensionEnabled(extension1->id()));
    EXPECT_FALSE(IsExtensionAcknowledged(extension1->id()));
  }

  {
    // The bubble shouldn't want to show (the extension that controls the home
    // page was acknowledged).
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    EXPECT_FALSE(bubble_delegate->ShouldShow());
  }

  // Disable the extension that was acknowledged.
  extension_service()->DisableExtension(
      extension2->id(), extensions::disable_reason::DISABLE_USER_ACTION);

  {
    auto bubble_delegate =
        std::make_unique<ControlledHomeBubbleDelegate>(browser());
    // Now a new extension controls the home page, so we should re-show the
    // bubble.
    EXPECT_TRUE(bubble_delegate->ShouldShow());
    EXPECT_EQ(extension1, bubble_delegate->extension_for_testing());
  }
}

TEST_F(ControlledHomeBubbleDelegateTest,
       PolicyExtensionsRequirePolicyIndicators) {
  scoped_refptr<const extensions::Extension> extension =
      LoadExtensionOverridingHome(
          "ext", extensions::mojom::ManifestLocation::kExternalPolicy);
  ASSERT_TRUE(extension);

  auto bubble_delegate =
      std::make_unique<ControlledHomeBubbleDelegate>(browser());
  // We still show the bubble for policy-installed extensions, but it should
  // have a policy decoration.
  EXPECT_TRUE(bubble_delegate->ShouldShow());

  EXPECT_EQ(u"", bubble_delegate->GetActionButtonText());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo> extra_view =
      bubble_delegate->GetExtraViewInfo();
  // Note: Mac vs Win message styling.
  EXPECT_TRUE(extra_view->text == u"Installed by your administrator" ||
              extra_view->text == u"Installed by Your Administrator");
  EXPECT_FALSE(extra_view->is_learn_more);
}
#endif
