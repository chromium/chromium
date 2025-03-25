// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/notebook_lm.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

namespace web_app {

ExternalInstallOptions GetConfigForNotebookLm() {
  static constexpr char kUrl[] = "https://notebooklm.google.com/install";
  ExternalInstallOptions options(
      /*install_url=*/GURL(kUrl),
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);
  options.add_to_applications_menu = true;
  options.add_to_search = true;
  options.expected_app_id = ash::kNotebookLmAppId;
  options.gate_on_feature = chromeos::features::kNotebookLmAppPreinstall.name;
  options.is_preferred_app_for_supported_links = true;
  options.user_type_allowlist = {apps::kUserTypeUnmanaged};

  return options;
}

}  // namespace web_app
