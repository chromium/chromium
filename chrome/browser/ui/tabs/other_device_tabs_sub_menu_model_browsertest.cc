// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/other_device_tabs_sub_menu_model.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"

namespace {

std::unique_ptr<AppMenuModel> CreateAppMenuModel(
    BrowserWindowInterface* target_browser) {
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, target_browser);
  app_menu_model->Init();
  return app_menu_model;
}

class OtherDeviceTabsSubMenuModelTest : public InProcessBrowserTest {
  base::test::ScopedFeatureList feature_list_{
      syncer::kCrossDeviceTabsInAppMenu};
};

class OtherDeviceTabsSubMenuModelFlagOffTest
    : public OtherDeviceTabsSubMenuModelTest {
 public:
  OtherDeviceTabsSubMenuModelFlagOffTest() {
    feature_list_.InitAndDisableFeature(syncer::kCrossDeviceTabsInAppMenu);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelFlagOffTest, NoAppMenuRow) {
  std::unique_ptr<AppMenuModel> app_menu_model = CreateAppMenuModel(browser());

  EXPECT_FALSE(
      app_menu_model
          ->GetIndexOfCommandId(AppMenuModel::kOtherDeviceTabsMenuPlaceholder)
          .has_value());
  EXPECT_FALSE(app_menu_model->IsCommandIdEnabled(IDC_OTHER_DEVICE_TABS_MENU));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest, AppMenuPlacement) {
  std::unique_ptr<AppMenuModel> app_menu_model = CreateAppMenuModel(browser());

  const std::optional<size_t> other_device_tabs_index =
      app_menu_model->GetIndexOfCommandId(
          AppMenuModel::kOtherDeviceTabsMenuPlaceholder);
  const std::optional<size_t> history_index =
      app_menu_model->GetIndexOfCommandId(
          AppMenuModel::kRecentTabsMenuPlaceholder);
  ASSERT_TRUE(other_device_tabs_index.has_value());
  ASSERT_TRUE(history_index.has_value());
  // "Other device tabs" sits directly below "History".
  EXPECT_EQ(history_index.value() + 1, other_device_tabs_index.value());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OTHER_DEVICE_TABS_MENU),
            app_menu_model->GetLabelAt(other_device_tabs_index.value()));
  EXPECT_FALSE(
      app_menu_model->GetIconAt(other_device_tabs_index.value()).IsEmpty());
  EXPECT_EQ(
      AppMenuModel::kOtherDeviceTabsMenuItem,
      app_menu_model->GetElementIdentifierAt(other_device_tabs_index.value()));
  // The row must be enabled or the views layer will not open the submenu.
  EXPECT_TRUE(app_menu_model->IsEnabledAt(other_device_tabs_index.value()));
  EXPECT_NE(nullptr,
            app_menu_model->GetSubmenuModelAt(other_device_tabs_index.value()));
}

IN_PROC_BROWSER_TEST_F(OtherDeviceTabsSubMenuModelTest,
                       NoAppMenuRowInIncognito) {
  std::unique_ptr<AppMenuModel> app_menu_model =
      CreateAppMenuModel(CreateIncognitoBrowser());

  EXPECT_FALSE(
      app_menu_model
          ->GetIndexOfCommandId(AppMenuModel::kOtherDeviceTabsMenuPlaceholder)
          .has_value());
}

}  // namespace
