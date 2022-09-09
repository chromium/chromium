// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"
#include "ui/views/view.h"

class ForceInstalledDeprecatedAppsDialogView : public views::View {
 public:
  METADATA_HEADER(ForceInstalledDeprecatedAppsDialogView);
  ForceInstalledDeprecatedAppsDialogView(
      const ForceInstalledDeprecatedAppsDialogView&) = delete;
  ForceInstalledDeprecatedAppsDialogView& operator=(
      const ForceInstalledDeprecatedAppsDialogView&) = delete;
  ~ForceInstalledDeprecatedAppsDialogView() override = default;

  // Create the dialog metadata and show it.
  static void CreateAndShowDialog(extensions::ExtensionId app_id,
                                  content::WebContents* web_contents,
                                  base::OnceClosure launch_anyways);

 private:
  ForceInstalledDeprecatedAppsDialogView(std::u16string app_name,
                                         content::WebContents* web_contents);

  std::u16string app_name_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_DEPRECATED_APPS_DIALOG_VIEW_H_
