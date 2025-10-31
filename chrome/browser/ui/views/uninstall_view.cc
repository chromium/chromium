// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

UninstallView::UninstallView(int* user_selection,
                             const base::RepeatingClosure& quit_closure)
    : user_selection_(*user_selection), quit_closure_(quit_closure) {
  SetupControls();
}

UninstallView::~UninstallView() {
  // Exit the message loop we were started with so that uninstall can continue.
  quit_closure_.Run();
}

void UninstallView::SetupControls() {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int checkbox_indent =
      provider->GetDistanceMetric(DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  const int related_vertical_small =
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  auto builder =
      views::Builder<UninstallView>(this)
          .SetButtonLabel(ui::mojom::DialogButton::kOk,
                          l10n_util::GetStringUTF16(IDS_UNINSTALL_BUTTON_TEXT))
          .SetTitle(IDS_UNINSTALL_CHROME)
          .SetAcceptCallback(base::BindOnce(&UninstallView::OnDialogAccepted,
                                            base::Unretained(this)))
          .SetCancelCallback(base::BindOnce(&UninstallView::OnDialogCancelled,
                                            base::Unretained(this)))
          .SetCloseCallback(base::BindOnce(&UninstallView::OnDialogCancelled,
                                           base::Unretained(this)))
          .set_margins(provider->GetDialogInsetsForContentType(
              views::DialogContentType::kText, views::DialogContentType::kText))
          .SetLayoutManager(std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kVertical))
          .AddChildren(
              // Message to confirm uninstallation.
              views::Builder<views::Label>()
                  .SetText(l10n_util::GetStringUTF16(IDS_UNINSTALL_VERIFY))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetProperty(
                      views::kMarginsKey,
                      gfx::Insets::TLBR(0, 0, unrelated_vertical_spacing, 0)),
              // The "delete profile" check box.
              views::Builder<views::Checkbox>()
                  .CopyAddressTo(&delete_profile_)
                  .SetText(
                      l10n_util::GetStringUTF16(IDS_UNINSTALL_DELETE_PROFILE))
                  .SetProperty(views::kMarginsKey,
                               gfx::Insets::TLBR(0, checkbox_indent, 0, 0)));

  std::move(builder)
      .AddChild(views::Builder<views::View>().SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(related_vertical_small, 0, 0, 0)))
      .BuildChildren();
}

void UninstallView::OnDialogAccepted() {
  *user_selection_ = content::RESULT_CODE_NORMAL_EXIT;
  if (delete_profile_->GetChecked()) {
    *user_selection_ = CHROME_RESULT_CODE_UNINSTALL_DELETE_PROFILE;
  }
}

void UninstallView::OnDialogCancelled() {
  *user_selection_ = CHROME_RESULT_CODE_UNINSTALL_USER_CANCEL;
}

BEGIN_METADATA(UninstallView)
END_METADATA

int ShowUninstallBrowserPrompt() {
  DCHECK(base::CurrentUIThread::IsSet());
  int result = content::RESULT_CODE_NORMAL_EXIT;

  base::RunLoop run_loop;
  UninstallView* view = new UninstallView(&result, run_loop.QuitClosure());
  views::DialogDelegate::CreateDialogWidget(view, NULL, NULL)->Show();
  run_loop.Run();
  return result;
}
