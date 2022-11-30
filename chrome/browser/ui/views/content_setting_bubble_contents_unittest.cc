// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/content_setting_bubble_contents.h"

#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class ContentSettingBubbleContentsTest : public ChromeViewsTestBase {
 public:
  Profile* profile() { return &profile_; }

 private:
  TestingProfile profile_;
};

class TestContentSettingBubbleModel : public ContentSettingBubbleModel {
 public:
  explicit TestContentSettingBubbleModel(content::WebContents* web_contents)
      : ContentSettingBubbleModel(nullptr, web_contents) {
    AddListItem(
        ListItem(nullptr, std::u16string(), std::u16string(), false, false, 0));
  }
};

class TestContentSettingBubbleContents : public ContentSettingBubbleContents {
 public:
  TestContentSettingBubbleContents(content::WebContents* web_contents,
                                   gfx::NativeView parent_window)
      : ContentSettingBubbleContents(
            std::make_unique<TestContentSettingBubbleModel>(web_contents),
            web_contents,
            nullptr,
            views::BubbleBorder::TOP_LEFT) {
    set_parent_window(parent_window);
  }

  // ContentSettingBubbleContents:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  }
};

// Regression test for http://crbug.com/1050801 .
TEST_F(ContentSettingBubbleContentsTest, NullDeref) {
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories;

  std::unique_ptr<views::Widget> parent_widget = CreateTestWidget();
  parent_widget->Show();

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Should not crash.
  std::unique_ptr<views::Widget> widget(
      views::BubbleDialogDelegateView::CreateBubble(
          std::make_unique<TestContentSettingBubbleContents>(
              web_contents.get(), parent_widget->GetNativeView())));
  widget->Show();
}
