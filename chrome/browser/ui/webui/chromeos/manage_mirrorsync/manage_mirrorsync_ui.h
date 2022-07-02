// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_

#include "ui/web_dialogs/web_dialog_ui.h"

namespace chromeos {

// The WebUI for chrome://manage-mirrorsync
class ManageMirrorSyncUI : public ui::MojoWebDialogUI {
 public:
  explicit ManageMirrorSyncUI(content::WebUI* web_ui);

  ManageMirrorSyncUI(const ManageMirrorSyncUI&) = delete;
  ManageMirrorSyncUI& operator=(const ManageMirrorSyncUI&) = delete;

  ~ManageMirrorSyncUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_UI_H_
