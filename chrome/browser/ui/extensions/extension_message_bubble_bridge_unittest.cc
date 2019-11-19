// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/test_toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return std::make_unique<extensions::ExtensionWebUIOverrideRegistrar>(context);
}

std::unique_ptr<KeyedService> BuildToolbarModel(
    content::BrowserContext* context) {
  return std::make_unique<ToolbarActionsModel>(
      Profile::FromBrowserContext(context),
      extensions::ExtensionPrefs::Get(context));
}

}  // namespace

class ExtensionMessageBubbleBridgeUnitTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  ExtensionMessageBubbleBridgeUnitTest() {}
  ~ExtensionMessageBubbleBridgeUnitTest() override {}
  Browser* browser() { return browser_.get(); }

 private:
  void SetUp() override {
    ExtensionServiceTestWithInstall::SetUp();
    InitializeEmptyExtensionService();

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_ = std::make_unique<Browser>(params);

    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()
        ->SetTestingFactory(browser()->profile(),
                            base::BindRepeating(&BuildOverrideRegistrar));
    extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(
        browser()->profile());
    ToolbarActionsModelFactory::GetInstance()->SetTestingFactory(
        browser()->profile(), base::BindRepeating(&BuildToolbarModel));
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestWithInstall::TearDown();
  }

  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleBridgeUnitTest);
};

TEST_F(ExtensionMessageBubbleBridgeUnitTest,
       TestGetExtraViewInfoMethodWithNormalSettingsOverrideExtension) {
  base::FilePath path(data_dir().AppendASCII("api_test/override/newtab/"));
  EXPECT_NE(nullptr, PackAndInstallCRX(path, INSTALL_NEW));

  std::unique_ptr<extensions::ExtensionMessageBubbleController>
      ntp_bubble_controller(new extensions::ExtensionMessageBubbleController(
          new extensions::NtpOverriddenBubbleDelegate(browser()->profile()),
          browser()));

  ASSERT_EQ(1U, ntp_bubble_controller->GetExtensionList().size());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(ntp_bubble_controller)));

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = bridge->GetExtraViewInfo();

  EXPECT_FALSE(extra_view_info->resource);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_LEARN_MORE), extra_view_info->text);
  EXPECT_TRUE(extra_view_info->is_learn_more);

  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_EXTENSION_CONTROLLED_RESTORE_SETTINGS),
      bridge->GetActionButtonText());
}

TEST_F(ExtensionMessageBubbleBridgeUnitTest,
       TestGetExtraViewInfoMethodWithPolicyInstalledSettingsOverrideExtension) {
  base::FilePath path(data_dir().AppendASCII("api_test/override/newtab/"));
  EXPECT_NE(nullptr,
            PackAndInstallCRX(path, extensions::Manifest::EXTERNAL_POLICY,
                              INSTALL_NEW));

  std::unique_ptr<extensions::ExtensionMessageBubbleController>
      ntp_bubble_controller(new extensions::ExtensionMessageBubbleController(
          new extensions::NtpOverriddenBubbleDelegate(browser()->profile()),
          browser()));

  ASSERT_EQ(1U, ntp_bubble_controller->GetExtensionList().size());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge(
      new ExtensionMessageBubbleBridge(std::move(ntp_bubble_controller)));

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = bridge->GetExtraViewInfo();

  extra_view_info = bridge->GetExtraViewInfo();

  EXPECT_EQ(&vector_icons::kBusinessIcon, extra_view_info->resource);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN),
            extra_view_info->text);
  EXPECT_FALSE(extra_view_info->is_learn_more);

  EXPECT_EQ(base::string16(), bridge->GetActionButtonText());
}

// Tests the ExtensionMessageBubbleBridge in conjunction with the
// SuspiciousExtensionBubbleDelegate.
TEST_F(ExtensionMessageBubbleBridgeUnitTest, SuspiciousExtensionBubble) {
  // Load up a simple extension.
  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "foo")
          .Set("description", "some extension")
          .Set("version", "0.1")
          .Set("manifest_version", 2);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder().SetID(crx_file::id_util::GenerateId("foo"))
                                    .SetManifest(manifest.Build())
                                    .Build();
  ASSERT_TRUE(extension);
  service()->AddExtension(extension.get());
  const std::string id = extension->id();
  ASSERT_TRUE(registry()->enabled_extensions().GetByID(id));

  // Disable the extension for being from outside the webstore.
  service()->DisableExtension(extension->id(),
                              extensions::disable_reason::DISABLE_NOT_VERIFIED);
  EXPECT_TRUE(registry()->disabled_extensions().GetByID(id));

  // Create a new message bubble; it should want to display for the disabled
  // extension. (Note: The bubble logic itself is tested more thoroughly in
  // extension_message_bubble_controller_unittest.cc.)
  auto suspicious_bubble_controller =
      std::make_unique<extensions::ExtensionMessageBubbleController>(
          new extensions::SuspiciousExtensionBubbleDelegate(profile()),
          browser());
  EXPECT_TRUE(suspicious_bubble_controller->ShouldShow());
  ASSERT_EQ(1u, suspicious_bubble_controller->GetExtensionIdList().size());
  EXPECT_EQ(id, suspicious_bubble_controller->GetExtensionIdList()[0]);

  // Create a new bridge and poke at a few of the methods to verify they are
  // correct and that nothing crashes.
  std::unique_ptr<ToolbarActionsBarBubbleDelegate> bridge =
      std::make_unique<ExtensionMessageBubbleBridge>(
          std::move(suspicious_bubble_controller));
  EXPECT_TRUE(bridge->ShouldShow());
  EXPECT_FALSE(bridge->ShouldCloseOnDeactivate());

  std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
      extra_view_info = bridge->GetExtraViewInfo();

  ASSERT_TRUE(extra_view_info);
  EXPECT_FALSE(extra_view_info->text.empty());
  EXPECT_TRUE(extra_view_info->is_learn_more);
  EXPECT_FALSE(extra_view_info->resource);
}
