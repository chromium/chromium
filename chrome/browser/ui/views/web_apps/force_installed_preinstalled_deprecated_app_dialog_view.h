// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APP_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APP_DIALOG_VIEW_H_

#include <string>

#include "base/auto_reset.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/layout/box_layout_view.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

class ForceInstalledPreinstalledDeprecatedAppDialogView
    : public views::BoxLayoutView {
 public:
  struct LinkConfig {
    GURL link;
    std::u16string link_text;
  };

  METADATA_HEADER(ForceInstalledPreinstalledDeprecatedAppDialogView);
  ForceInstalledPreinstalledDeprecatedAppDialogView(
      const ForceInstalledPreinstalledDeprecatedAppDialogView&) = delete;
  ForceInstalledPreinstalledDeprecatedAppDialogView& operator=(
      const ForceInstalledPreinstalledDeprecatedAppDialogView&) = delete;
  ~ForceInstalledPreinstalledDeprecatedAppDialogView() override = default;

  // Create the dialog metadata and shows it. This will CHECK-fail if the
  // extension id is not installed or is not a preinstalled app id (see
  // `extensions::IsPreinstalledAppId`).
  static void CreateAndShowDialog(const extensions::ExtensionId& extension_id,
                                  content::WebContents* web_contents);

  static base::AutoReset<absl::optional<LinkConfig>>
  SetOverrideLinkConfigForTesting(const LinkConfig& link_config);

 private:
  ForceInstalledPreinstalledDeprecatedAppDialogView(
      const std::u16string& app_name,
      const GURL& app_link,
      const std::u16string& link_string,
      content::WebContents* web_contents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FORCE_INSTALLED_PREINSTALLED_DEPRECATED_APP_DIALOG_VIEW_H_
