// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_dialog.h"

#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace {

// TODO(b/237066325): Update this once UI has landed for this.
constexpr int kDialogWidth = 768;
constexpr int kDialogHeight = 608;

GURL GetURL() {
  return GURL{chrome::kChromeUIManageMirrorSyncURL};
}

}  // namespace

namespace ash {

void ManageMirrorSyncDialog::Show(Profile* profile) {
  auto* instance = SystemWebDialogDelegate::FindInstance(GetURL().spec());
  if (instance) {
    instance->Focus();
    return;
  }

  instance = new ManageMirrorSyncDialog(profile);
  instance->ShowSystemDialog();
}

ManageMirrorSyncDialog::ManageMirrorSyncDialog(Profile* profile)
    : SystemWebDialogDelegate(GetURL(), /*title=*/{}) {}

ManageMirrorSyncDialog::~ManageMirrorSyncDialog() = default;

void ManageMirrorSyncDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(::kDialogWidth, ::kDialogHeight);
}

void ManageMirrorSyncDialog::OnDialogShown(content::WebUI* webui) {
  mirrorsync_ui_ = static_cast<ManageMirrorSyncUI*>(webui->GetController());
  return SystemWebDialogDelegate::OnDialogShown(webui);
}

void ManageMirrorSyncDialog::OnCloseContents(content::WebContents* source,
                                             bool* out_close_dialog) {
  mirrorsync_ui_ = nullptr;
  return SystemWebDialogDelegate::OnCloseContents(source, out_close_dialog);
}

void ManageMirrorSyncDialog::OnWebContentsFinishedLoad() {
  DCHECK(dialog_window());
  // TODO(b/237066325): Localize this string.
  dialog_window()->SetTitle(u"Manage MirrorSync");
}

}  // namespace ash
