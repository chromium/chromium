// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/update_recommended_message_box.h"

#include "build/build_config.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/chromium_strings.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/first_run/upgrade_util_mac.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, public:

// static
void UpdateRecommendedMessageBox::Show(gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  constrained_window::CreateBrowserModalDialogViews(
      new UpdateRecommendedMessageBox(), parent_window)->Show();
}

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, private:

UpdateRecommendedMessageBox::UpdateRecommendedMessageBox() {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_RELAUNCH_AND_UPDATE));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_NOT_NOW));
  base::string16 update_message;
#if defined(OS_CHROMEOS)
  update_message = l10n_util::GetStringUTF16(IDS_UPDATE_RECOMMENDED);
#else
  update_message = l10n_util::GetPluralStringFUTF16(
      IDS_UPDATE_RECOMMENDED, BrowserList::GetIncognitoBrowserCount());
#endif

  views::MessageBoxView::InitParams params(update_message);
  params.message_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      ChromeDistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  // Also deleted when the window closes.
  message_box_view_ = new views::MessageBoxView(params);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::UPDATE_RECOMMENDED);
}

UpdateRecommendedMessageBox::~UpdateRecommendedMessageBox() {
}

bool UpdateRecommendedMessageBox::Accept() {
#if defined(OS_MACOSX)
  if (!upgrade_util::ShouldContinueToRelaunchForUpgrade())
    return false;  // Leave the dialog up for the user to return to.
#endif             // OS_MACOSX

  chrome::AttemptRelaunch();
  return true;
}

bool UpdateRecommendedMessageBox::ShouldShowWindowTitle() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

bool UpdateRecommendedMessageBox::ShouldShowCloseButton() const {
  return false;
}

base::string16 UpdateRecommendedMessageBox::GetWindowTitle() const {
#if defined(OS_CHROMEOS)
  return base::string16();
#else
  return l10n_util::GetStringUTF16(IDS_UPDATE_RECOMMENDED_DIALOG_TITLE);
#endif
}

void UpdateRecommendedMessageBox::DeleteDelegate() {
  delete this;
}

ui::ModalType UpdateRecommendedMessageBox::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

views::View* UpdateRecommendedMessageBox::GetContentsView() {
  return message_box_view_;
}

views::Widget* UpdateRecommendedMessageBox::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* UpdateRecommendedMessageBox::GetWidget() const {
  return message_box_view_->GetWidget();
}
