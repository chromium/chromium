// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

namespace {

class CheckActiveWebContentsMenuModel : public ui::MenuModel {
 public:
  explicit CheckActiveWebContentsMenuModel(TabStripModel* tab_strip_model)
      : tab_strip_model_(tab_strip_model) {
    DCHECK(tab_strip_model_);
  }
  CheckActiveWebContentsMenuModel(const CheckActiveWebContentsMenuModel&) =
      delete;
  CheckActiveWebContentsMenuModel& operator=(
      const CheckActiveWebContentsMenuModel&) = delete;
  ~CheckActiveWebContentsMenuModel() override = default;

  // ui::MenuModel:
  bool HasIcons() const override { return false; }
  int GetItemCount() const override {
    EXPECT_TRUE(tab_strip_model_->GetActiveWebContents());
    return 0;
  }
  ItemType GetTypeAt(int index) const override { return TYPE_COMMAND; }
  int GetCommandIdAt(int index) const override { return 0; }
  base::string16 GetLabelAt(int index) const override {
    return base::string16();
  }
  bool IsItemDynamicAt(int index) const override { return false; }
  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    return false;
  }
  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }
  bool IsItemCheckedAt(int index) const override { return false; }
  int GetGroupIdAt(int index) const override { return 0; }
  bool GetIconAt(int index, gfx::Image* icon) const override { return false; }
  ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const override {
    return nullptr;
  }
  bool IsEnabledAt(int index) const override { return false; }
  ui::MenuModel* GetSubmenuModelAt(int index) const override { return nullptr; }
  void ActivatedAt(int index) override {}

 private:
  TabStripModel* const tab_strip_model_;
};

class TestParentView : public views::View {
 public:
  explicit TestParentView(gfx::NativeWindow context) {
    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = context;
    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));
  }
  TestParentView(const TestParentView&) = delete;
  TestParentView& operator=(const TestParentView&) = delete;
  ~TestParentView() override = default;

  const views::Widget* GetWidget() const override { return widget_.get(); }

 private:
  std::unique_ptr<views::Widget> widget_;
};

}  // namespace

class ToolbarButtonViewsTest : public ChromeViewsTestBase {
 public:
  ToolbarButtonViewsTest() {}
  ToolbarButtonViewsTest(const ToolbarButtonViewsTest&) = delete;
  ToolbarButtonViewsTest& operator=(const ToolbarButtonViewsTest&) = delete;
};

TEST_F(ToolbarButtonViewsTest, MenuDoesNotShowWhenTabStripIsEmpty) {
  TestTabStripModelDelegate delegate;
  TestingProfile profile;
  TabStripModel tab_strip(&delegate, &profile);
  EXPECT_FALSE(tab_strip.GetActiveWebContents());

  auto model = std::make_unique<CheckActiveWebContentsMenuModel>(&tab_strip);
  ToolbarButton* button =
      new ToolbarButton(nullptr, std::move(model), &tab_strip);

  // ToolbarButton needs a parent view on some platforms. |parent_view| takes
  // ownership of |button|.
  TestParentView parent_view(GetContext());
  parent_view.AddChildView(button);

  // Since |tab_strip| is empty, calling this does not do anything. This is the
  // expected result. If it actually tries to show the menu, then
  // CheckActiveWebContentsMenuModel::GetItemCount() will get called and the
  // EXPECT_TRUE() call inside will fail.
  button->ShowContextMenuForView(nullptr, gfx::Point(), ui::MENU_SOURCE_MOUSE);
}
