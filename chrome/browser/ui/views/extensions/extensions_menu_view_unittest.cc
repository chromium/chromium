// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_menu_view.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_button.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

class ExtensionsMenuViewUnitTest : public BrowserWithTestWindowTest {
 public:
  ExtensionsMenuViewUnitTest()
      : allow_extension_menu_instances_(
            ExtensionsMenuView::AllowInstancesForTesting()) {
    feature_list_.InitAndEnableFeature(features::kExtensionsToolbarMenu);
  }
  ~ExtensionsMenuViewUnitTest() override = default;

  void SetUp() override;
  void TearDown() override;

  // Adds a simple extension to the profile.
  scoped_refptr<const extensions::Extension> AddSimpleExtension(
      const char* name);

  extensions::ExtensionService* extension_service() {
    return extension_service_;
  }

  ExtensionsMenuView* extensions_menu() { return extensions_menu_.get(); }

 private:
  base::AutoReset<bool> allow_extension_menu_instances_;
  base::test::ScopedFeatureList feature_list_;

  extensions::ExtensionService* extension_service_ = nullptr;

  std::unique_ptr<ExtensionsToolbarContainer> extensions_container_;
  std::unique_ptr<ExtensionsMenuView> extensions_menu_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsMenuViewUnitTest);
};

void ExtensionsMenuViewUnitTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();

  // Set up some extension-y bits.
  extensions::LoadErrorReporter::Init(false);
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

  extension_service_ =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  extensions::extension_action_test_util::CreateToolbarModelForProfile(
      profile());

  // And create the menu itself.
  extensions_container_ =
      std::make_unique<ExtensionsToolbarContainer>(browser());
  extensions_container_->set_owned_by_client();

  extensions_menu_ = std::make_unique<ExtensionsMenuView>(
      nullptr, browser(), extensions_container_.get());
  extensions_menu_->set_owned_by_client();
}

void ExtensionsMenuViewUnitTest::TearDown() {
  extensions_menu_ = nullptr;
  extensions_container_ = nullptr;

  BrowserWithTestWindowTest::TearDown();
}

scoped_refptr<const extensions::Extension>
ExtensionsMenuViewUnitTest::AddSimpleExtension(const char* name) {
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(name).Build();
  extension_service()->AddExtension(extension.get());

  return extension;
}

TEST_F(ExtensionsMenuViewUnitTest, ExtensionsAreShownInTheMenu) {
  // To start, there should be no extensions in the menu.
  EXPECT_EQ(0u, extensions_menu()->extensions_menu_items_for_testing().size());

  // Add an extension, and verify that it's added to the menu.
  constexpr char kExtensionName[] = "Test 1";
  AddSimpleExtension(kExtensionName);

  {
    std::vector<ExtensionsMenuItemView*> menu_items =
        extensions_menu()->extensions_menu_items_for_testing();
    ASSERT_EQ(1u, menu_items.size());
    EXPECT_EQ(kExtensionName,
              base::UTF16ToUTF8(menu_items[0]
                                    ->primary_action_button_for_testing()
                                    ->label_text_for_testing()));
  }
}
