// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_controller_views.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_dialog_view_test_api.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/hover_list_view.h"
#include "chrome/browser/ui/webauthn/authenticator_request_sheet_model.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/metrics.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

class TestSheetModel : public AuthenticatorRequestSheetModel {
 public:
  TestSheetModel() = default;

  TestSheetModel(const TestSheetModel&) = delete;
  TestSheetModel& operator=(const TestSheetModel&) = delete;

  ~TestSheetModel() override = default;

  // Getters for data on step specific content:
  std::u16string GetStepSpecificLabelText() { return u"Test Label"; }

  int accept_count() const { return accept_count_; }

 private:
  // AuthenticatorRequestSheetModel:
  bool IsActivityIndicatorVisible() const override { return true; }
  bool IsCancelButtonVisible() const override { return true; }
  std::u16string GetCancelButtonLabel() const override {
    return u"Test Cancel";
  }

  AcceptButtonState GetAcceptButtonState() const override {
    return AcceptButtonState::kEnabled;
  }
  std::u16string GetAcceptButtonLabel() const override { return u"Test OK"; }

  std::u16string GetStepTitle() const override { return u"Test Title"; }

  std::u16string GetStepDescription() const override {
    return u"Test Description That Is Super Long So That It No Longer Fits On "
           u"One Line Because Life Would Be Just Too Simple That Way";
  }

  std::u16string GetError() const override {
    return u"You must construct additional pylons.";
  }

  void OnBack() override {}
  void OnAccept() override { accept_count_++; }
  void OnCancel() override {}

 private:
  int accept_count_ = 0;
};

class TestSheetView : public AuthenticatorRequestSheetView {
 public:
  explicit TestSheetView(std::unique_ptr<TestSheetModel> model)
      : AuthenticatorRequestSheetView(std::move(model)) {
    ReInitChildViews();
  }

  TestSheetView(const TestSheetView&) = delete;
  TestSheetView& operator=(const TestSheetView&) = delete;

  ~TestSheetView() override = default;

 private:
  TestSheetModel* test_sheet_model() {
    return static_cast<TestSheetModel*>(model());
  }

  // AuthenticatorRequestSheetView:
  std::pair<std::unique_ptr<views::View>, AutoFocus> BuildStepSpecificContent()
      override {
    return std::make_pair(std::make_unique<views::Label>(
                              test_sheet_model()->GetStepSpecificLabelText()),
                          AutoFocus::kNo);
  }
};

HoverListView* FindHoverListView(views::View* root) {
  if (!root) {
    return nullptr;
  }
  if (auto* hover_list = views::AsViewClass<HoverListView>(root)) {
    return hover_list;
  }
  for (views::View* child : root->children()) {
    if (HoverListView* found = FindHoverListView(child)) {
      return found;
    }
  }
  return nullptr;
}

views::Button* FindTopListItemButton(views::View* root) {
  if (!root) {
    return nullptr;
  }
  if (auto* button = views::AsViewClass<views::Button>(root)) {
    return button;
  }
  for (views::View* child : root->children()) {
    if (views::Button* found = FindTopListItemButton(child)) {
      return found;
    }
  }
  return nullptr;
}

}  // namespace

class StepTransitionObserver
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  StepTransitionObserver() = default;
  int step_transition_count() { return step_transition_count_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { step_transition_count_++; }

 private:
  int step_transition_count_ = 0;
};

class AuthenticatorDialogViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void TearDownOnMainThread() override {
    view_controller_.reset();
    DialogBrowserTest::TearDownOnMainThread();
  }

  void ShowUi(const std::string& name) override {
    content::WebContents* const web_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();
    CHECK(web_contents);

    dialog_model_->relying_party_id = "example.com";
    // Set the step to a view that is capable of displaying a dialog:
    dialog_model_->SetStep(AuthenticatorRequestDialogModel::Step::kTimedOut);
    StepTransitionObserver step_transition_observer;
    dialog_model_->AddObserver(&step_transition_observer);

    view_controller_ =
        std::make_unique<AuthenticatorRequestDialogViewControllerViews>(
            web_contents, dialog_model_.get());

    if (name == "default") {
      test::AuthenticatorRequestDialogViewTestApi::SetSheetTo(
          view_controller_.get(),
          std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
      EXPECT_EQ(step_transition_observer.step_transition_count(), 0);
    } else if (name == "ReplaceSheet") {
      test::AuthenticatorRequestDialogViewTestApi::SetSheetTo(
          view_controller_.get(),
          std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
      // Replace it immediately to trigger the dangling pointer scenario
      test::AuthenticatorRequestDialogViewTestApi::SetSheetTo(
          view_controller_.get(),
          std::make_unique<TestSheetView>(std::make_unique<TestSheetModel>()));
    }

    dialog_model_->RemoveObserver(&step_transition_observer);
  }

 private:
  scoped_refptr<AuthenticatorRequestDialogModel> dialog_model_ =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  std::unique_ptr<AuthenticatorRequestDialogViewControllerViews>
      view_controller_;
};

// Test the dialog with a custom delegate.
IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, InvokeUi_ReplaceSheet) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest,
                       HoverListViewInputEventProtection) {
  content::WebContents* const web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  CHECK(web_contents);

  int mechanism_callback_count_ = 0;
  auto dialog_model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  dialog_model->relying_party_id = "example.com";
  dialog_model->mechanisms.emplace_back(
      AuthenticatorRequestDialogModel::Mechanism::Transport(
          AuthenticatorTransport::kUsbHumanInterfaceDevice),
      u"Security key", kPasskeyUsbDarkCustomIcon,
      base::BindRepeating(
          base::BindLambdaForTesting([&]() { mechanism_callback_count_++; })));
  dialog_model->SetStep(
      AuthenticatorRequestDialogModel::Step::kMechanismSelection);

  auto view_controller =
      std::make_unique<AuthenticatorRequestDialogViewControllerViews>(
          web_contents, dialog_model.get());

  // Trigger OnStepTransition() so that view_controller creates the sheet for
  // the current step and calls Show().
  view_controller->OnStepTransition();

  AuthenticatorRequestSheetView* sheet =
      test::AuthenticatorRequestDialogViewTestApi::GetSheet(
          view_controller.get());
  ASSERT_TRUE(sheet);

  HoverListView* hover_list_view = FindHoverListView(sheet);
  ASSERT_TRUE(hover_list_view);

  views::Button* top_row_button = FindTopListItemButton(hover_list_view);
  ASSERT_TRUE(top_row_button);

  ui::MouseEvent click_event(ui::EventType::kMousePressed, gfx::Point(),
                             gfx::Point(), base::TimeTicks::Now(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);

  views::test::ButtonTestApi(top_row_button).NotifyClick(click_event);
  EXPECT_EQ(mechanism_callback_count_, 0);

  ASSERT_TRUE(sheet->GetWidget());
  auto* dialog_delegate =
      sheet->GetWidget()->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  auto* dialog_client_view = dialog_delegate->GetDialogClientView();
  ASSERT_TRUE(dialog_client_view);

  dialog_client_view->ResetViewShownTimeStampForTesting();
  views::test::ButtonTestApi(top_row_button).NotifyClick(click_event);
  EXPECT_EQ(mechanism_callback_count_, 1);
}

IN_PROC_BROWSER_TEST_F(AuthenticatorDialogViewTest, KeyEventInputProtection) {
  content::WebContents* const web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  CHECK(web_contents);

  auto dialog_model =
      base::MakeRefCounted<AuthenticatorRequestDialogModel>(nullptr);
  dialog_model->relying_party_id = "example.com";
  dialog_model->SetStep(AuthenticatorRequestDialogModel::Step::kTimedOut);

  auto view_controller =
      std::make_unique<AuthenticatorRequestDialogViewControllerViews>(
          web_contents, dialog_model.get());

  auto sheet_model = std::make_unique<TestSheetModel>();
  auto* model_ptr = sheet_model.get();
  test::AuthenticatorRequestDialogViewTestApi::SetSheetTo(
      view_controller.get(),
      std::make_unique<TestSheetView>(std::move(sheet_model)));

  auto* sheet = test::AuthenticatorRequestDialogViewTestApi::GetSheet(
      view_controller.get());
  ASSERT_TRUE(sheet);
  auto* dialog_delegate =
      sheet->GetWidget()->widget_delegate()->AsDialogDelegate();
  ASSERT_TRUE(dialog_delegate);
  views::MdTextButton* ok_button = dialog_delegate->GetOkButton();
  ASSERT_TRUE(ok_button);

  EXPECT_FALSE(dialog_delegate->ShouldAllowKeyEventsDuringInputProtection());

  // Key event immediately upon dialog shown should be ignored.
  ui::KeyEvent enter(ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE,
                     ui::EventTimeForNow());
  views::test::ButtonTestApi(ok_button).NotifyClick(enter);
  EXPECT_EQ(model_ptr->accept_count(), 0);

  // Fast-forward input protection for the first sheet.
  dialog_delegate->GetDialogClientView()
      ->ResetViewShownTimeStampForTesting();

  // Replace with a second sheet; input protection should be re-armed.
  auto sheet_model2 = std::make_unique<TestSheetModel>();
  auto* model_ptr2 = sheet_model2.get();
  test::AuthenticatorRequestDialogViewTestApi::SetSheetTo(
      view_controller.get(),
      std::make_unique<TestSheetView>(std::move(sheet_model2)));

  ok_button = dialog_delegate->GetOkButton();
  ASSERT_TRUE(ok_button);
  views::test::ButtonTestApi(ok_button).NotifyClick(enter);
  EXPECT_EQ(model_ptr2->accept_count(), 0);

  // After the protection interval, key events should be accepted.
  ui::KeyEvent enter_delayed(
      ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::EF_NONE,
      ui::EventTimeForNow() + views::GetDoubleClickInterval());
  views::test::ButtonTestApi(ok_button).NotifyClick(enter_delayed);
  EXPECT_EQ(model_ptr2->accept_count(), 1);
}
