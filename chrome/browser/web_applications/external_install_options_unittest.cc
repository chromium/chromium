// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_install_options.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

ExternalInstallOptions ConvertParamsToExternalInstallOptions(
    const WebAppInstallParams& install_params) {
  ExternalInstallOptions install_options(
      /*install_url=*/install_params.fallback_start_url,
      /*user_display_mode=*/install_params.user_display_mode,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  install_options.force_reinstall = install_params.force_reinstall;

  if (install_params.fallback_app_name.has_value()) {
    install_options.fallback_app_name =
        base::UTF16ToUTF8(install_params.fallback_app_name.value());
  }

  install_options.add_to_applications_menu =
      install_params.add_to_applications_menu;
  install_options.add_to_desktop = install_params.add_to_desktop;
  install_options.add_to_quick_launch_bar =
      install_params.add_to_quick_launch_bar;
  install_options.add_to_search = install_params.add_to_search;
  install_options.add_to_management = install_params.add_to_management;
  install_options.is_disabled = install_params.is_disabled;

  install_options.require_manifest = install_params.require_manifest;

  install_options.additional_search_terms =
      install_params.additional_search_terms;

  install_options.launch_query_params = install_params.launch_query_params;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  install_options.system_app_type = install_params.system_app_type;
#endif

  install_options.oem_installed = install_params.oem_installed;

  return install_options;
}

}  // namespace

TEST(WebAppExternalInstallOptions,
     ConvertExternalInstallOptionsToParams_DefaultConstructor) {
  ExternalInstallOptions install_options(
      /*install_url=*/GURL{"https://example.org"},
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  WebAppInstallParams install_params =
      ConvertExternalInstallOptionsToParams(install_options);

  ExternalInstallOptions converted_install_options =
      ConvertParamsToExternalInstallOptions(install_params);

  EXPECT_EQ(converted_install_options, install_options);
}

TEST(WebAppExternalInstallOptions,
     ConvertExternalInstallOptionsToParams_NotDefaultConstructor) {
  ExternalInstallOptions install_options(
      /*install_url=*/GURL{"https://example.com"},
      /*user_display_mode=*/mojom::UserDisplayMode::kStandalone,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  // The values below are deliberately different from default
  // ExternalInstallOptions constructor values.

  install_options.force_reinstall = true;
  install_options.user_display_mode = mojom::UserDisplayMode::kStandalone;
  install_options.fallback_app_name = "Fallback App Name";

  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;
  install_options.add_to_search = false;
  install_options.add_to_management = false;
  install_options.is_disabled = true;

  install_options.require_manifest = true;

  install_options.additional_search_terms = {"str1", "str2"};

  install_options.launch_query_params = "param";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  install_options.system_app_type = ash::SystemWebAppType::SAMPLE;
#endif

  install_options.oem_installed = true;

  WebAppInstallParams install_params =
      ConvertExternalInstallOptionsToParams(install_options);

  ExternalInstallOptions converted_install_options =
      ConvertParamsToExternalInstallOptions(install_params);

  EXPECT_EQ(converted_install_options, install_options);
}

}  // namespace web_app
