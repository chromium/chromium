// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr int kDialogHeightDp = 512;
constexpr int kDialogWidthDp = 462;

}  // namespace

// static
ParentAccessDialog::ShowError ParentAccessDialog::Show() {
  ParentAccessDialog* current_instance = GetInstance();
  if (current_instance) {
    return ShowError::kDialogAlreadyVisible;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile->IsChild()) {
    return ShowError::kNotAChildUser;
  }

  // Note:  |current_instance|'s memory is freed when
  // SystemWebDialogDelegate::OnDialogClosed() is called.
  current_instance = new ParentAccessDialog();
  current_instance->ShowSystemDialogForBrowserContext(profile);
  return ShowError::kNone;
}

// static
ParentAccessDialog* ParentAccessDialog::GetInstance() {
  return static_cast<ParentAccessDialog*>(
      SystemWebDialogDelegate::FindInstance(chrome::kChromeUIParentAccessURL));
}

ui::ModalType ParentAccessDialog::GetDialogModalType() const {
  return ui::ModalType::MODAL_TYPE_SYSTEM;
}

void ParentAccessDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidthDp, kDialogHeightDp);
}

bool ParentAccessDialog::ShouldCloseDialogOnEscape() const {
  return true;
}

ParentAccessDialog::ParentAccessDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIParentAccessURL),
                              /*title=*/std::u16string()) {}

ParentAccessDialog::~ParentAccessDialog() = default;

}  // namespace chromeos
