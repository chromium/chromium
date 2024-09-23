// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/installer/util/shell_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
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
  const int related_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int related_horizontal_spacing =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
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

  // Set default browser combo box. If the default should not or cannot be
  // changed, widgets are not shown. We assume here that if Chrome cannot
  // be set programatically as default, neither can any other browser (for
  // instance because the OS doesn't permit that).
  if (ShellUtil::CanMakeChromeDefaultUnattended() &&
      shell_integration::GetDefaultBrowser() == shell_integration::IS_DEFAULT) {
    browsers_ = std::make_unique<BrowsersMap>();
    ShellUtil::GetRegisteredBrowsers(browsers_.get());
    if (!browsers_->empty()) {
      builder.AddChildren(
          views::Builder<views::View>().SetProperty(
              views::kMarginsKey,
              gfx::Insets::TLBR(related_vertical_spacing, 0, 0, 0)),
          views::Builder<views::BoxLayoutView>()
              .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
              .SetBetweenChildSpacing(related_horizontal_spacing)
              .AddChildren(
                  views::Builder<views::Checkbox>()
                      .CopyAddressTo(&change_default_browser_)
                      .SetText(l10n_util::GetStringUTF16(
                          IDS_UNINSTALL_SET_DEFAULT_BROWSER))
                      .SetCallback(base::BindRepeating(
                          [](UninstallView* view) {
                            view->browsers_combo_->SetEnabled(
                                view->change_default_browser_->GetChecked());
                          },
                          base::Unretained(this)))
                      .SetProperty(views::kMarginsKey,
                                   gfx::Insets::TLBR(0, checkbox_indent, 0, 0)),
                  views::Builder<views::Combobox>()
                      .CopyAddressTo(&browsers_combo_)
                      .SetModel(this)
                      .SetEnabled(false)));
    }
  }

  std::move(builder)
      .AddChild(views::Builder<views::View>().SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(related_vertical_small, 0, 0, 0)))
      .BuildChildren();
}

void UninstallView::OnDialogAccepted() {
  *user_selection_ = content::RESULT_CODE_NORMAL_EXIT;
  if (delete_profile_->GetChecked()) {
    *user_selection_ = chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE;
  }
  if (change_default_browser_ && change_default_browser_->GetChecked()) {
    BrowsersMap::const_iterator i = browsers_->begin();
    std::advance(i, browsers_combo_->GetSelectedIndex().value());
    base::LaunchOptions options;
    options.start_hidden = true;
    base::LaunchProcess(i->second, options);
  }
}

void UninstallView::OnDialogCancelled() {
  *user_selection_ = chrome::RESULT_CODE_UNINSTALL_USER_CANCEL;
}

size_t UninstallView::GetItemCount() const {
  DCHECK(!browsers_->empty());
  return browsers_->size();
}

std::u16string UninstallView::GetItemAt(size_t index) const {
  DCHECK_LT(index, browsers_->size());
  BrowsersMap::const_iterator i = browsers_->begin();
  std::advance(i, index);
  return base::WideToUTF16(i->first);
}

BEGIN_METADATA(UninstallView)
END_METADATA

namespace chrome {

int ShowUninstallBrowserPrompt() {
  DCHECK(base::CurrentUIThread::IsSet());
  int result = content::RESULT_CODE_NORMAL_EXIT;

  base::RunLoop run_loop;
  UninstallView* view = new UninstallView(&result, run_loop.QuitClosure());
  views::DialogDelegate::CreateDialogWidget(view, NULL, NULL)->Show();
  run_loop.Run();
  return result;
}

}  // namespace chrome
