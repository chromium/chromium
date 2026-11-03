// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_focus_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class AppMenuFocusControllerTest : public ChromeViewsTestBase {
 public:
  AppMenuFocusControllerTest() = default;
  ~AppMenuFocusControllerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    owner_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    owner_widget_->Show();

    menu_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    search_bar_ =
        menu_widget_->SetContentsView(std::make_unique<AppMenuSearchBarView>());
    menu_widget_->Show();
  }

  void TearDown() override {
    search_bar_ = nullptr;
    menu_widget_.reset();
    owner_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Textfield* GetSearchField() {
    return views::AsViewClass<views::Textfield>(search_bar_->GetViewByElementId(
        AppMenuSearchBarView::kSearchFieldElementId));
  }

  std::unique_ptr<views::Widget> owner_widget_;
  std::unique_ptr<views::Widget> menu_widget_;
  raw_ptr<AppMenuSearchBarView> search_bar_ = nullptr;
};

TEST_F(AppMenuFocusControllerTest, ClearsAndRestoresOwnerFocus) {
  auto* browser_textfield =
      owner_widget_->SetContentsView(std::make_unique<views::Textfield>());
  browser_textfield->RequestFocus();
  ASSERT_TRUE(browser_textfield->HasFocus());

  {
    AppMenuFocusController controller(owner_widget_.get(), search_bar_);
    // Constructing the controller clears focus in the owning browser widget so
    // the browser view and menu search bar do not both appear focused.
    EXPECT_FALSE(browser_textfield->HasFocus());
    EXPECT_EQ(owner_widget_->GetFocusManager()->GetFocusedView(), nullptr);
  }

  // Destroying the controller (or calling RestoreOwnerFocus(true)) restores
  // focus to the previously focused browser view.
  EXPECT_TRUE(browser_textfield->HasFocus());

  // When RestoreOwnerFocus(false) is called (e.g. a deferred menu action is
  // about to execute), focus is not restored to the previous view.
  {
    AppMenuFocusController controller(owner_widget_.get(), search_bar_);
    EXPECT_FALSE(browser_textfield->HasFocus());
    controller.RestoreOwnerFocus(/*should_restore=*/false);
    EXPECT_FALSE(browser_textfield->HasFocus());
  }
  EXPECT_FALSE(browser_textfield->HasFocus());
}

TEST_F(AppMenuFocusControllerTest,
       PreservesWebViewFocusWithoutBlurAndRestoresTextInputClient) {
  TestingProfile profile;
  auto* web_view = owner_widget_->SetContentsView(
      std::make_unique<views::WebView>(&profile));
  web_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  web_view->RequestFocus();
  ASSERT_TRUE(web_view->HasFocus());

  ui::DummyTextInputClient web_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  ui::InputMethod* const input_method = owner_widget_->GetInputMethod();
  ASSERT_NE(input_method, nullptr);
  input_method->SetFocusedTextInputClient(&web_text_input_client);
  ASSERT_EQ(input_method->GetTextInputClient(), &web_text_input_client);

  {
    AppMenuFocusController controller(owner_widget_.get(), search_bar_);

    // WebView retains FocusManager focus so the web page does not receive a DOM
    // blur event, while its TextInputClient is detached from the browser's
    // InputMethod while the menu is open.
    EXPECT_TRUE(web_view->HasFocus());
    EXPECT_EQ(owner_widget_->GetFocusManager()->GetFocusedView(), web_view);
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);
  }

  // Closing the menu restores the web page's TextInputClient without having
  // blurred WebView.
  EXPECT_TRUE(web_view->HasFocus());
  EXPECT_EQ(input_method->GetTextInputClient(), &web_text_input_client);
  input_method->DetachTextInputClient(&web_text_input_client);
}

TEST_F(AppMenuFocusControllerTest,
       DropsSavedWebViewTextInputClientWhenDestroyedOrInvalidated) {
  TestingProfile profile;
  content::RenderViewHostTestEnabler rvh_test_enabler;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(&profile, nullptr);

  auto* web_view = owner_widget_->SetContentsView(
      std::make_unique<views::WebView>(&profile));
  web_view->SetWebContents(web_contents.get());
  web_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  web_view->RequestFocus();
  ASSERT_TRUE(web_view->HasFocus());

  ui::InputMethod* const input_method = owner_widget_->GetInputMethod();
  ASSERT_NE(input_method, nullptr);

  // Case 1: The detached TextInputClient itself is destroyed while the menu is
  // open. RestoreOwnerFocus must not re-attach the destroyed client.
  {
    auto web_text_input_client =
        std::make_unique<ui::DummyTextInputClient>(ui::TEXT_INPUT_TYPE_TEXT);
    input_method->SetFocusedTextInputClient(web_text_input_client.get());
    ASSERT_EQ(input_method->GetTextInputClient(), web_text_input_client.get());

    AppMenuFocusController controller(owner_widget_.get(), search_bar_);
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);

    web_text_input_client.reset();
    controller.RestoreOwnerFocus();
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);
  }

  // Case 2: The WebView's WebContents is detached while the menu is open.
  // OnWebContentsDetached drops the saved client even if the client object is
  // still alive.
  {
    ui::DummyTextInputClient web_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
    input_method->SetFocusedTextInputClient(&web_text_input_client);
    ASSERT_EQ(input_method->GetTextInputClient(), &web_text_input_client);

    AppMenuFocusController controller(owner_widget_.get(), search_bar_);
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);

    web_view->SetWebContents(nullptr);
    controller.RestoreOwnerFocus();
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);

    web_view->SetWebContents(web_contents.get());
    web_view->RequestFocus();
    ASSERT_TRUE(web_view->HasFocus());
  }

  // Case 3: The WebContents is destroyed while the menu is open.
  // WebContentsDestroyed drops the saved client.
  {
    ui::DummyTextInputClient web_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
    input_method->SetFocusedTextInputClient(&web_text_input_client);
    ASSERT_EQ(input_method->GetTextInputClient(), &web_text_input_client);

    AppMenuFocusController controller(owner_widget_.get(), search_bar_);
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);

    web_view->SetWebContents(nullptr);
    web_contents.reset();
    controller.RestoreOwnerFocus();
    EXPECT_EQ(input_method->GetTextInputClient(), nullptr);
  }
}

TEST_F(AppMenuFocusControllerTest, PreserveSearchBarFocus) {
  AppMenuFocusController controller(owner_widget_.get(), search_bar_);
  views::Textfield* const search_field = GetSearchField();
  ASSERT_NE(search_field, nullptr);

  // When the search field is not focused, PreserveSearchBarFocus() does not
  // focus it on scope exit.
  ASSERT_FALSE(search_field->HasFocus());
  {
    base::ScopedClosureRunner preserve_focus =
        controller.PreserveSearchBarFocus();
  }
  EXPECT_FALSE(search_field->HasFocus());

  // When the search field is focused, PreserveSearchBarFocus() restores focus
  // after focus is cleared during a menu rebuild.
  search_field->RequestFocus();
  ASSERT_TRUE(search_field->HasFocus());
  {
    base::ScopedClosureRunner preserve_focus =
        controller.PreserveSearchBarFocus();
    menu_widget_->GetFocusManager()->ClearFocus();
    EXPECT_FALSE(search_field->HasFocus());
  }
  EXPECT_TRUE(search_field->HasFocus());
}

}  // namespace
