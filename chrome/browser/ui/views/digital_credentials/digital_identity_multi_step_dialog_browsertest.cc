// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/digital_credentials/digital_identity_multi_step_dialog.h"

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

using ButtonParams = ui::DialogModel::Button::Params;

// Returns whether `parent` or one of its children is a views::Label with text
// `find_text`.
bool HasChildLabelViewWithText(const views::View* parent,
                               const std::u16string& find_text) {
  for (views::View* child : parent->children()) {
    if (child->GetObjectName() == "Label") {
      const views::Label* child_label = static_cast<const views::Label*>(child);
      if (child_label->GetText() == find_text) {
        return true;
      }
    }

    if (HasChildLabelViewWithText(child, find_text)) {
      return true;
    }
  }
  return false;
}

// Observes visibility changes on the `widget` passed to the constructor.
class WidgetVisibilityObserver : public views::WidgetObserver {
 public:
  explicit WidgetVisibilityObserver(views::Widget* widget) {
    observer_.Observe(widget);
  }

  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override {
    did_widget_visibility_change_ = true;
  }

  bool did_widget_visiblity_change() const {
    return did_widget_visibility_change_;
  }

 private:
  bool did_widget_visibility_change_ = false;

  base::ScopedObservation<views::Widget, WidgetVisibilityObserver> observer_{
      this};
};

}  // anonymous namespace

class DigitalIdentityMultiStepDialogBrowserTest : public InProcessBrowserTest {
 public:
  DigitalIdentityMultiStepDialogBrowserTest() = default;
  ~DigitalIdentityMultiStepDialogBrowserTest() override = default;

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebIdentityDigitalCredentials};
};

// Check that DigitalIdentityMultiStepDialog::TryShow() updates the dialog's
// contents if the dialog is already showing.
IN_PROC_BROWSER_TEST_F(DigitalIdentityMultiStepDialogBrowserTest,
                       UpdateDialog) {
  constexpr char16_t kStep1Title[] = u"Title Step 1";
  constexpr char16_t kStep1Body[] = u"Body Step 1";
  constexpr char16_t kStep1AcceptButton[] = u"Accept Step 1";
  constexpr char16_t kStep1CancelButton[] = u"Cancel Step 1";

  constexpr char16_t kStep2Title[] = u"Title Step 1";
  constexpr char16_t kStep2Body[] = u"Body Step 1";
  constexpr char16_t kStep2AcceptButton[] = u"Accept Step 1";
  constexpr char16_t kStep2CancelButton[] = u"Cancel Step 1";

  auto dialog = std::make_unique<DigitalIdentityMultiStepDialog>(
      GetActiveWebContents()->GetWeakPtr());
  DigitalIdentityMultiStepDialog::TestApi dialog_test_api(dialog.get());

  {
    std::optional<ButtonParams> accept_button_params =
        std::make_optional<ButtonParams>();
    accept_button_params->SetLabel(kStep1AcceptButton);

    ButtonParams cancel_button_params;
    cancel_button_params.SetLabel(kStep1CancelButton);

    dialog->TryShow(accept_button_params, base::DoNothing(),
                    cancel_button_params, base::DoNothing(), kStep1Title,
                    kStep1Body, nullptr);
  }

  views::Widget* widget = dialog_test_api.get_widget();
  views::BubbleDialogDelegate* widget_delegate =
      dialog_test_api.get_widget_delegate();

  // Observe `widget` to ensure that it does not get hidden as a result of the
  // second DigitalIdentityMultiStepDialog::TryShow() call.
  auto visibility_observer = std::make_unique<WidgetVisibilityObserver>(widget);

  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(kStep1Title, widget_delegate->GetWindowTitle());
  EXPECT_TRUE(HasChildLabelViewWithText(widget->GetRootView(), kStep1Body));
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            widget_delegate->buttons());
  EXPECT_EQ(kStep1AcceptButton, widget_delegate->GetDialogButtonLabel(
                                    ui::mojom::DialogButton::kOk));
  EXPECT_EQ(kStep1CancelButton, widget_delegate->GetDialogButtonLabel(
                                    ui::mojom::DialogButton::kCancel));

  {
    std::optional<ButtonParams> accept_button_params =
        std::make_optional<ButtonParams>();
    accept_button_params->SetLabel(kStep2AcceptButton);

    ButtonParams cancel_button_params;
    cancel_button_params.SetLabel(kStep2CancelButton);

    dialog->TryShow(accept_button_params, base::DoNothing(),
                    cancel_button_params, base::DoNothing(), kStep2Title,
                    kStep2Body, nullptr);
  }

  // The same widget should be showing.
  EXPECT_EQ(widget, dialog_test_api.get_widget());
  EXPECT_FALSE(visibility_observer->did_widget_visiblity_change());

  EXPECT_EQ(kStep2Title, widget_delegate->GetWindowTitle());
  EXPECT_TRUE(HasChildLabelViewWithText(widget->GetRootView(), kStep2Body));
  EXPECT_EQ(static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel),
            widget_delegate->buttons());
  EXPECT_EQ(kStep2AcceptButton, widget_delegate->GetDialogButtonLabel(
                                    ui::mojom::DialogButton::kOk));
  EXPECT_EQ(kStep2CancelButton, widget_delegate->GetDialogButtonLabel(
                                    ui::mojom::DialogButton::kCancel));
}

// Check that pressing the "OK" button in the dialog does not run the
// "cancel/close" callback.
IN_PROC_BROWSER_TEST_F(DigitalIdentityMultiStepDialogBrowserTest,
                       NotCanceledAfterOk) {
  bool was_ok_callback_called = false;
  bool was_cancel_callback_called = false;

  auto ok_callback = [](bool* was_ok_callback_called) {
    *was_ok_callback_called = true;
  };

  auto cancel_callback = [](bool* was_cancel_callback_called) {
    *was_cancel_callback_called = true;
  };

  auto dialog = std::make_unique<DigitalIdentityMultiStepDialog>(
      GetActiveWebContents()->GetWeakPtr());
  auto dialog_test_api =
      std::make_unique<DigitalIdentityMultiStepDialog::TestApi>(dialog.get());
  dialog->TryShow(std::make_optional<ButtonParams>(),
                  base::BindRepeating(ok_callback, &was_ok_callback_called),
                  ButtonParams(),
                  base::BindOnce(cancel_callback, &was_cancel_callback_called),
                  u"Title", u"Body", nullptr);
  EXPECT_TRUE(dialog_test_api->get_widget()->IsVisible());

  // Accept dialog and run any pending tasks.
  dialog_test_api->get_widget_delegate()->AcceptDialog();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(was_ok_callback_called);

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      dialog_test_api->get_widget());
  dialog_test_api.reset();
  dialog.reset();
  destroyed_waiter.Wait();

  EXPECT_FALSE(was_cancel_callback_called);
}

// Check that toggling button enabled state via DialogModel::Button::Params
// works.
IN_PROC_BROWSER_TEST_F(DigitalIdentityMultiStepDialogBrowserTest,
                       DisableButton) {
  auto dialog = std::make_unique<DigitalIdentityMultiStepDialog>(
      GetActiveWebContents()->GetWeakPtr());
  DigitalIdentityMultiStepDialog::TestApi dialog_test_api(dialog.get());

  {
    std::optional<ButtonParams> accept_button_params =
        std::make_optional<ButtonParams>();
    accept_button_params->SetEnabled(false);
    dialog->TryShow(accept_button_params, base::DoNothing(), ButtonParams(),
                    base::DoNothing(), u"Title", u"Body", nullptr);
    EXPECT_FALSE(dialog_test_api.get_widget_delegate()->IsDialogButtonEnabled(
        ui::mojom::DialogButton::kOk));
  }

  {
    std::optional<ButtonParams> accept_button_params =
        std::make_optional<ButtonParams>();
    accept_button_params->SetEnabled(true);
    dialog->TryShow(accept_button_params, base::DoNothing(), ButtonParams(),
                    base::DoNothing(), u"Title", u"Body", nullptr);
    EXPECT_TRUE(dialog_test_api.get_widget_delegate()->IsDialogButtonEnabled(
        ui::mojom::DialogButton::kOk));
  }
}
