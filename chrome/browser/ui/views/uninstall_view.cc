// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include "base/message_loop/message_loop_current.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/installer/util/shell_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

UninstallView::UninstallView(int* user_selection,
                             const base::Closure& quit_closure)
    : confirm_label_(NULL),
      delete_profile_(NULL),
      change_default_browser_(NULL),
      browsers_combo_(NULL),
      user_selection_(*user_selection),
      quit_closure_(quit_closure) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_UNINSTALL_BUTTON_TEXT));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::TEXT, views::TEXT));
  SetupControls();
}

UninstallView::~UninstallView() {
  // Exit the message loop we were started with so that uninstall can continue.
  quit_closure_.Run();

  // Delete Combobox as it holds a reference to us.
  delete browsers_combo_;
}

void UninstallView::SetupControls() {
  using views::ColumnSet;

  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());

  // Message to confirm uninstallation.
  int column_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
  auto confirm_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_UNINSTALL_VERIFY));
  confirm_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  confirm_label_ = layout->AddView(std::move(confirm_label));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  const int checkbox_indent = provider->GetDistanceMetric(
      DISTANCE_SUBSECTION_HORIZONTAL_INDENT);
  const int unrelated_vertical_spacing =
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL);
  const int related_vertical_spacing = provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_VERTICAL);
  const int related_horizontal_spacing = provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const int related_vertical_small = provider->GetDistanceMetric(
      DISTANCE_RELATED_CONTROL_VERTICAL_SMALL);

  layout->AddPaddingRow(views::GridLayout::kFixedSize,
                        unrelated_vertical_spacing);

  // The "delete profile" check box.
  ++column_set_id;
  column_set = layout->AddColumnSet(column_set_id);
  column_set->AddPaddingColumn(views::GridLayout::kFixedSize, checkbox_indent);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        views::GridLayout::kFixedSize,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
  delete_profile_ = layout->AddView(std::make_unique<views::Checkbox>(
      l10n_util::GetStringUTF16(IDS_UNINSTALL_DELETE_PROFILE)));

  // Set default browser combo box. If the default should not or cannot be
  // changed, widgets are not shown. We assume here that if Chrome cannot
  // be set programatically as default, neither can any other browser (for
  // instance because the OS doesn't permit that).
  if (ShellUtil::CanMakeChromeDefaultUnattended() &&
      shell_integration::GetDefaultBrowser() == shell_integration::IS_DEFAULT) {
    browsers_ = std::make_unique<BrowsersMap>();
    ShellUtil::GetRegisteredBrowsers(browsers_.get());
    if (!browsers_->empty()) {
      layout->AddPaddingRow(views::GridLayout::kFixedSize,
                            related_vertical_spacing);

      ++column_set_id;
      column_set = layout->AddColumnSet(column_set_id);
      column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                   checkbox_indent);
      column_set->AddColumn(
          views::GridLayout::LEADING, views::GridLayout::CENTER,
          views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(views::GridLayout::kFixedSize,
                                   related_horizontal_spacing);
      column_set->AddColumn(
          views::GridLayout::LEADING, views::GridLayout::CENTER,
          views::GridLayout::kFixedSize, views::GridLayout::USE_PREF, 0, 0);
      layout->StartRow(views::GridLayout::kFixedSize, column_set_id);
      change_default_browser_ =
          layout->AddView(std::make_unique<views::Checkbox>(
              l10n_util::GetStringUTF16(IDS_UNINSTALL_SET_DEFAULT_BROWSER),
              this));
      browsers_combo_ =
          layout->AddView(std::make_unique<views::Combobox>(this));
      browsers_combo_->SetEnabled(false);
    }
  }

  layout->AddPaddingRow(views::GridLayout::kFixedSize, related_vertical_small);
}

bool UninstallView::Accept() {
  user_selection_ = service_manager::RESULT_CODE_NORMAL_EXIT;
  if (delete_profile_->GetChecked())
    user_selection_ = chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE;
  if (change_default_browser_ && change_default_browser_->GetChecked()) {
    BrowsersMap::const_iterator i = browsers_->begin();
    std::advance(i, browsers_combo_->GetSelectedIndex());
    base::LaunchOptions options;
    options.start_hidden = true;
    base::LaunchProcess(i->second, options);
  }
  return true;
}

bool UninstallView::Cancel() {
  user_selection_ = chrome::RESULT_CODE_UNINSTALL_USER_CANCEL;
  return true;
}

void UninstallView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  if (change_default_browser_ == sender) {
    // Disable the browsers combobox if the user unchecks the checkbox.
    DCHECK(browsers_combo_);
    browsers_combo_->SetEnabled(change_default_browser_->GetChecked());
  }
}

base::string16 UninstallView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_UNINSTALL_CHROME);
}

int UninstallView::GetItemCount() const {
  DCHECK(!browsers_->empty());
  return browsers_->size();
}

base::string16 UninstallView::GetItemAt(int index) {
  DCHECK_LT(index, static_cast<int>(browsers_->size()));
  BrowsersMap::const_iterator i = browsers_->begin();
  std::advance(i, index);
  return i->first;
}

namespace chrome {

int ShowUninstallBrowserPrompt() {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  int result = service_manager::RESULT_CODE_NORMAL_EXIT;

  base::RunLoop run_loop;
  UninstallView* view = new UninstallView(&result,
                                          run_loop.QuitClosure());
  views::DialogDelegate::CreateDialogWidget(view, NULL, NULL)->Show();
  run_loop.Run();
  return result;
}

}  // namespace chrome
