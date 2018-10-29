// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_ui.h"

#include <memory>

#include "base/command_line.h"
#include "base/i18n/message_formatter.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/android_about_app_info.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/version_handler_chromeos.h"
#endif

#if defined(OS_WIN)
#include "chrome/install_static/install_details.h"
#endif

using content::WebUIDataSource;

namespace {

WebUIDataSource* CreateVersionUIDataSource() {
  WebUIDataSource* html_source =
      WebUIDataSource::Create(chrome::kChromeUIVersionHost);

  // Localized and data strings.
  html_source->AddLocalizedString(version_ui::kTitle, IDS_VERSION_UI_TITLE);
  html_source->AddLocalizedString(version_ui::kApplicationLabel,
                                  IDS_PRODUCT_NAME);
  html_source->AddString(version_ui::kVersion,
                         version_info::GetVersionNumber());
  html_source->AddString(version_ui::kVersionModifier,
                         chrome::GetChannelName());
  html_source->AddString(version_ui::kJSEngine, "V8");
  html_source->AddString(version_ui::kJSVersion, V8_VERSION_STRING);
  html_source->AddLocalizedString(version_ui::kCompany,
                                  IDS_ABOUT_VERSION_COMPANY_NAME);
  html_source->AddString(
      version_ui::kCopyright,
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));
  html_source->AddLocalizedString(version_ui::kRevision,
                                  IDS_VERSION_UI_REVISION);
  html_source->AddString(version_ui::kCL, version_info::GetLastChange());
  html_source->AddLocalizedString(version_ui::kOfficial,
                                  version_info::IsOfficialBuild()
                                      ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  html_source->AddLocalizedString(version_ui::kUserAgentName,
                                  IDS_VERSION_UI_USER_AGENT);
  html_source->AddString(version_ui::kUserAgent, GetUserAgent());
  html_source->AddLocalizedString(version_ui::kCommandLineName,
                                  IDS_VERSION_UI_COMMAND_LINE);
  // Note that the executable path and profile path are retrieved asynchronously
  // and returned in VersionHandler::OnGotFilePaths. The area is initially
  // blank.
  html_source->AddLocalizedString(version_ui::kExecutablePathName,
                                  IDS_VERSION_UI_EXECUTABLE_PATH);
  html_source->AddString(version_ui::kExecutablePath, std::string());
  html_source->AddLocalizedString(version_ui::kProfilePathName,
                                  IDS_VERSION_UI_PROFILE_PATH);
  html_source->AddString(version_ui::kProfilePath, std::string());
  html_source->AddLocalizedString(version_ui::kVariationsName,
                                  IDS_VERSION_UI_VARIATIONS);
  html_source->AddLocalizedString(version_ui::kVariationsCmdName,
                                  IDS_VERSION_UI_VARIATIONS_CMD);
#if defined(OS_CHROMEOS)
  html_source->AddLocalizedString(version_ui::kARC, IDS_ARC_LABEL);
  html_source->AddLocalizedString(version_ui::kPlatform, IDS_PLATFORM_LABEL);
  html_source->AddLocalizedString(version_ui::kCustomizationId,
                                  IDS_VERSION_UI_CUSTOMIZATION_ID);
  html_source->AddLocalizedString(version_ui::kFirmwareVersion,
                                  IDS_VERSION_UI_FIRMWARE_VERSION);
#else
  html_source->AddLocalizedString(version_ui::kOSName, IDS_VERSION_UI_OS);
  html_source->AddString(version_ui::kOSType, version_info::GetOSType());
#endif  // OS_CHROMEOS

#if defined(OS_ANDROID)
  html_source->AddString(version_ui::kOSVersion,
                         AndroidAboutAppInfo::GetOsInfo());
  html_source->AddLocalizedString(version_ui::kGmsName, IDS_VERSION_UI_GMS);
  html_source->AddString(version_ui::kGmsVersion,
                         AndroidAboutAppInfo::GetGmsInfo());
#else
  html_source->AddString(version_ui::kFlashPlugin, "Flash");
  // Note that the Flash version is retrieve asynchronously and returned in
  // VersionHandler::OnGotPlugins. The area is initially blank.
  html_source->AddString(version_ui::kFlashVersion, std::string());
#endif  // OS_ANDROID

  html_source->AddLocalizedString(
      version_ui::kVersionBitSize,
      sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT);

#if defined(OS_WIN)
  html_source->AddString(
      version_ui::kCommandLine,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
#elif defined(OS_POSIX)
  std::string command_line;
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (auto iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  html_source->AddString(version_ui::kCommandLine, command_line);
#endif

#if defined(OS_WIN)
  base::string16 update_cohort_name =
      install_static::InstallDetails::Get().update_cohort_name();
  if (!update_cohort_name.empty()) {
    html_source->AddString(version_ui::kUpdateCohortName,
                           l10n_util::GetStringFUTF16(
                               IDS_VERSION_UI_COHORT_NAME, update_cohort_name));
  } else {
    html_source->AddString(version_ui::kUpdateCohortName, std::string());
  }
#endif  // defined(OS_WIN)

  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath(version_ui::kVersionJS, IDR_VERSION_UI_JS);
  html_source->AddResourcePath(version_ui::kAboutVersionCSS,
                               IDR_VERSION_UI_CSS);
  html_source->SetDefaultResource(IDR_VERSION_UI_HTML);
  html_source->UseGzip();
  return html_source;
}

}  // namespace

VersionUI::VersionUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(std::make_unique<VersionHandlerChromeOS>());
#else
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
#endif

#if !defined(OS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  WebUIDataSource::Add(profile, CreateVersionUIDataSource());
}

VersionUI::~VersionUI() {
}
