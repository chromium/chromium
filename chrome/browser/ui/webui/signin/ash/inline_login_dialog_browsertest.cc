// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"

#include "base/json/json_reader.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {
namespace {

// Subclass to access protected constructor and protected methods.
class TestInlineLoginDialog : public InlineLoginDialog {
 public:
  TestInlineLoginDialog() = default;
  using SystemWebDialogDelegate::dialog_window;
};

// A simulated modal dialog. Taking focus seems important to repro the crash,
// but I'm not sure why.
class ChildModalDialogDelegate : public views::DialogDelegateView {
 public:
  ChildModalDialogDelegate() {
    SetModalType(ui::mojom::ModalType::kChild);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // Dialogs that take focus must have a name and role to pass accessibility
    // checks.
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName("Test dialog",
                                   ax::mojom::NameFrom::kAttribute);
  }
  ChildModalDialogDelegate(const ChildModalDialogDelegate&) = delete;
  ChildModalDialogDelegate& operator=(const ChildModalDialogDelegate&) = delete;
  ~ChildModalDialogDelegate() override = default;
};

}  // namespace

using InlineLoginDialogTest = InProcessBrowserTest;

// Regression test for use-after-free and crash. https://1170577
IN_PROC_BROWSER_TEST_F(InlineLoginDialogTest,
                       CanOpenChildModelDialogThenCloseParent) {
  // Show the dialog. It is owned by the views system.
  TestInlineLoginDialog* login_dialog = new TestInlineLoginDialog();
  login_dialog->ShowSystemDialog();

  // Create a child modal dialog, similar to an http auth modal dialog.
  content::WebContents* web_contents =
      login_dialog->GetWebUIForTest()->GetWebContents();
  ASSERT_TRUE(web_contents);
  // The ChildModalDialogDelegate is owned by the views system.
  constrained_window::ShowWebModalDialogViews(new ChildModalDialogDelegate,
                                              web_contents);

  // Close the parent window.
  views::Widget* login_widget =
      views::Widget::GetWidgetForNativeWindow(login_dialog->dialog_window());
  views::test::WidgetDestroyedWaiter waiter(login_widget);
  login_dialog->Close();
  waiter.Wait();

  // No crash.
}

IN_PROC_BROWSER_TEST_F(InlineLoginDialogTest, ReturnsEmptyDialogArgs) {
  auto* dialog = new InlineLoginDialog(
      GURL(chrome::kChromeUIChromeSigninURL), /*options=*/std::nullopt,
      /*close_dialog_closure=*/base::DoNothing());
  EXPECT_TRUE(InlineLoginDialog::IsShown());
  EXPECT_EQ(dialog->GetDialogArgs(), "");

  // Delete dialog by calling OnDialogClosed.
  dialog->OnDialogClosed("");
  // Make sure the dialog is deleted.
  EXPECT_FALSE(InlineLoginDialog::IsShown());
}

IN_PROC_BROWSER_TEST_F(InlineLoginDialogTest, ReturnsCorrectDialogArgs) {
  account_manager::AccountAdditionOptions options;
  options.is_available_in_arc = true;
  options.show_arc_availability_picker = false;
  auto* dialog =
      new InlineLoginDialog(GURL(chrome::kChromeUIChromeSigninURL), options,
                            /*close_dialog_closure=*/base::DoNothing());
  EXPECT_TRUE(InlineLoginDialog::IsShown());

  std::optional<base::Value> args =
      base::JSONReader::Read(dialog->GetDialogArgs());
  ASSERT_TRUE(args.has_value());
  EXPECT_TRUE(args.value().is_dict());
  const base::Value::Dict& dict = args.value().GetDict();
  std::optional<bool> is_available_in_arc = dict.FindBool("isAvailableInArc");
  std::optional<bool> show_arc_availability_picker =
      dict.FindBool("showArcAvailabilityPicker");
  ASSERT_TRUE(is_available_in_arc.has_value());
  ASSERT_TRUE(show_arc_availability_picker.has_value());
  EXPECT_EQ(is_available_in_arc.value(), options.is_available_in_arc);
  EXPECT_EQ(show_arc_availability_picker.value(),
            options.show_arc_availability_picker);

  // Delete dialog by calling OnDialogClosed.
  dialog->OnDialogClosed("");
  // Make sure the dialog is deleted.
  EXPECT_FALSE(InlineLoginDialog::IsShown());
}

}  // namespace ash
