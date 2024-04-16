// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/create_shortcut_delegate.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace shortcuts {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CreateShortcutDelegate,
                                      kCreateShortcutDialogOkButtonId);

CreateShortcutDelegate::CreateShortcutDelegate(
    content::WebContents* web_contents,
    chrome::CreateShortcutDialogCallback final_callback)
    : content::WebContentsObserver(web_contents),
      final_callback_(std::move(final_callback)) {}

CreateShortcutDelegate::~CreateShortcutDelegate() = default;

void CreateShortcutDelegate::OnAccept() {
  if (final_callback_) {
    base::RecordAction(
        base::UserMetricsAction("CreateDesktopShortcutDialogAccepted"));
    CHECK(!text_field_data_.empty());
    std::move(final_callback_).Run(true, text_field_data_);
  }
}

void CreateShortcutDelegate::OnClose() {
  if (final_callback_) {
    base::RecordAction(
        base::UserMetricsAction("CreateDesktopShortcutDialogCancelled"));
    std::move(final_callback_).Run(false, text_field_data_);
  }
}

void CreateShortcutDelegate::OnTitleUpdated(
    const std::u16string& trimmed_text_field_data) {
  text_field_data_ = trimmed_text_field_data;
  ui::DialogModel::Button* ok_button =
      dialog_model()->GetButtonByUniqueId(kCreateShortcutDialogOkButtonId);
  CHECK(ok_button);
  dialog_model()->SetButtonEnabled(ok_button,
                                   /*enabled=*/!text_field_data_.empty());
}

void CreateShortcutDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    CloseDialogAsIgnored();
  }
}

void CreateShortcutDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void CreateShortcutDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Close dialog when navigating to a different document.
  if (!navigation_handle->IsSameDocument()) {
    CloseDialogAsIgnored();
  }
}

void CreateShortcutDelegate::CloseDialogAsIgnored() {
  if (dialog_model() && dialog_model()->host()) {
    dialog_model()->host()->Close();
  }
}

}  // namespace shortcuts
