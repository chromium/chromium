// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"

class Profile;

namespace ash {

class ManageMirrorSyncUI;

class ManageMirrorSyncDialog : public SystemWebDialogDelegate {
 public:
  static void Show(Profile* profile);

 private:
  explicit ManageMirrorSyncDialog(Profile* profile);
  ~ManageMirrorSyncDialog() override;

  // SystemWebDialogDelegate:
  void GetDialogSize(gfx::Size* size) const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  void OnWebContentsFinishedLoad() override;
  raw_ptr<ManageMirrorSyncUI> mirrorsync_ui_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_DIALOG_H_
