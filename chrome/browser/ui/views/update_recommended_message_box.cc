// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/update_recommended_message_box.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
  SetButtonLabel(ui::DIALOG_BUTTON_OK,
                 l10n_util::GetStringUTF16(IDS_RELAUNCH_AND_UPDATE));
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                 l10n_util::GetStringUTF16(IDS_NOT_NOW));
  SetModalType(ui::MODAL_TYPE_WINDOW);
  SetOwnedByWidget(true);
  SetTitle(IDS_UPDATE_RECOMMENDED_DIALOG_TITLE);
  std::u16string update_message;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  update_message = l10n_util::GetStringUTF16(IDS_UPDATE_RECOMMENDED);
#else
  update_message = l10n_util::GetPluralStringFUTF16(
      IDS_UPDATE_RECOMMENDED, BrowserList::GetIncognitoBrowserCount());
#endif

  // Also deleted when the window closes.
  message_box_view_ = new views::MessageBoxView(update_message);
  message_box_view_->SetMessageWidth(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
}

UpdateRecommendedMessageBox::~UpdateRecommendedMessageBox() {
}

bool UpdateRecommendedMessageBox::Accept() {
  chrome::AttemptRelaunch();
  return true;
}

bool UpdateRecommendedMessageBox::ShouldShowWindowTitle() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  return true;
#endif
}

bool UpdateRecommendedMessageBox::ShouldShowCloseButton() const {
  return false;
}

views::View* UpdateRecommendedMessageBox::GetContentsView() {
  return message_box_view_;
}

views::Widget* UpdateRecommendedMessageBox::GetWidget() {
  return message_box_view_ ? message_box_view_->GetWidget() : nullptr;
}

const views::Widget* UpdateRecommendedMessageBox::GetWidget() const {
  return message_box_view_ ? message_box_view_->GetWidget() : nullptr;
}
