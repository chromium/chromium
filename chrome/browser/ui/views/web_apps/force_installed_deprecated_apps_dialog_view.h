// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "extensions/common/extension_id.h"
#include "ui/views/layout/box_layout_view.h"

namespace content {
class WebContents;
}  // namespace content

class ForceInstalledDeprecatedAppsDialogView : public views::BoxLayoutView {
 public:
  METADATA_HEADER(ForceInstalledDeprecatedAppsDialogView);
  ForceInstalledDeprecatedAppsDialogView(
      const ForceInstalledDeprecatedAppsDialogView&) = delete;
  ForceInstalledDeprecatedAppsDialogView& operator=(
      const ForceInstalledDeprecatedAppsDialogView&) = delete;
  ~ForceInstalledDeprecatedAppsDialogView() override = default;

  // Create the dialog metadata and show it.
  static void CreateAndShowDialog(const extensions::ExtensionId& app_id,
                                  content::WebContents* web_contents,
                                  base::OnceClosure launch_anyways);

 private:
  ForceInstalledDeprecatedAppsDialogView(const std::u16string& app_name,
                                         content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_
