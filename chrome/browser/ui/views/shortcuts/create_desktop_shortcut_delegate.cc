// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut_delegate.h"

#include <string>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "chrome/browser/ui/views/shortcuts/create_desktop_shortcut.h"
#include "content/public/browser/page.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace shortcuts {
std::u16string AppendProfileNameToTitleIfNeeded(Profile* profile,
                                                std::u16string old_title) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager || profile_manager->GetNumberOfProfiles() <= 1) {
    return old_title;
  }

  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    return old_title;
  }

  base::TrimWhitespace(old_title, base::TRIM_ALL, &old_title);
  return base::StrCat({old_title, u" (", entry->GetName(), u")"});
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CreateDesktopShortcutDelegate,
                                      kCreateShortcutDialogOkButtonId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CreateDesktopShortcutDelegate,
                                      kCreateShortcutDialogTitleFieldId);

CreateDesktopShortcutDelegate::CreateDesktopShortcutDelegate(
    content::WebContents* web_contents,
    CreateShortcutDialogCallback final_callback)
    : content::WebContentsObserver(web_contents),
      final_callback_(std::move(final_callback)) {}

CreateDesktopShortcutDelegate::~CreateDesktopShortcutDelegate() = default;

void CreateDesktopShortcutDelegate::StartObservingForPictureInPictureOcclusion(
    views::Widget* dialog_widget) {
  occlusion_observation_.Observe(dialog_widget);
}

void CreateDesktopShortcutDelegate::OnAccept() {
  if (final_callback_) {
    base::RecordAction(
        base::UserMetricsAction("CreateDesktopShortcutDialogAccepted"));
    CHECK(!text_field_data_.empty());
    std::move(final_callback_).Run(text_field_data_);
  }
}

void CreateDesktopShortcutDelegate::OnClose() {
  if (final_callback_) {
    base::RecordAction(
        base::UserMetricsAction("CreateDesktopShortcutDialogCancelled"));
    std::move(final_callback_).Run(std::nullopt);
  }
}

void CreateDesktopShortcutDelegate::OnTitleUpdated(
    const std::u16string& trimmed_text_field_data) {
  text_field_data_ = trimmed_text_field_data;
  ui::DialogModel::Button* ok_button =
      dialog_model()->GetButtonByUniqueId(kCreateShortcutDialogOkButtonId);
  CHECK(ok_button);
  dialog_model()->SetButtonEnabled(ok_button,
                                   /*enabled=*/!text_field_data_.empty());
}

void CreateDesktopShortcutDelegate::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    CloseDialogAsIgnored();
  }
}

void CreateDesktopShortcutDelegate::WebContentsDestroyed() {
  CloseDialogAsIgnored();
}

void CreateDesktopShortcutDelegate::PrimaryPageChanged(content::Page& page) {
  CloseDialogAsIgnored();
}

void CreateDesktopShortcutDelegate::OnOcclusionStateChanged(bool occluded) {
  // If a picture-in-picture window is occluding the dialog, force it to close
  // to prevent spoofing.
  if (occluded) {
    PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
  }
}

void CreateDesktopShortcutDelegate::CloseDialogAsIgnored() {
  if (dialog_model() && dialog_model()->host()) {
    dialog_model()->host()->Close();
  }
}

}  // namespace shortcuts
