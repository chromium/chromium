// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_ui.h"

#include <memory>

#include "base/command_line.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version/version_handler.h"
#include "chrome/browser/ui/webui/version/version_util_win.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "v8/include/v8-version-string.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/android_about_app_info.h"
#else  // !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"
#endif

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/webui/version/version_handler_win.h"
#endif

using content::WebUIDataSource;

namespace {

WebUIDataSource* CreateVersionUIDataSource() {
  WebUIDataSource* html_source =
      WebUIDataSource::Create(chrome::kChromeUIVersionHost);
  // These localized strings are used to label version details.
  static constexpr webui::LocalizedString kStrings[] = {
    {version_ui::kTitle, IDS_VERSION_UI_TITLE},
    {version_ui::kLogoAltText, IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
    {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
    {version_ui::kCompany, IDS_ABOUT_VERSION_COMPANY_NAME},
    {version_ui::kRevision, IDS_VERSION_UI_REVISION},
    {version_ui::kUserAgentName, IDS_VERSION_UI_USER_AGENT},
    {version_ui::kCommandLineName, IDS_VERSION_UI_COMMAND_LINE},
    {version_ui::kExecutablePathName, IDS_VERSION_UI_EXECUTABLE_PATH},
    {version_ui::kProfilePathName, IDS_VERSION_UI_PROFILE_PATH},
    {version_ui::kVariationsName, IDS_VERSION_UI_VARIATIONS},
    {version_ui::kVariationsCmdName, IDS_VERSION_UI_VARIATIONS_CMD},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {version_ui::kARC, IDS_ARC_LABEL},
    {version_ui::kPlatform, IDS_PLATFORM_LABEL},
    {version_ui::kCustomizationId, IDS_VERSION_UI_CUSTOMIZATION_ID},
    {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
#else
    {version_ui::kOSName, IDS_VERSION_UI_OS},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if defined(OS_ANDROID)
    {version_ui::kGmsName, IDS_VERSION_UI_GMS},
#endif  // OS_ANDROID
  };
  html_source->AddLocalizedStrings(kStrings);

  VersionUI::AddVersionDetailStrings(html_source);

  html_source->UseStringsJs();
  html_source->AddResourcePath(version_ui::kVersionJS, IDR_VERSION_UI_JS);
  html_source->AddResourcePath(version_ui::kAboutVersionCSS,
                               IDR_VERSION_UI_CSS);
#if defined(OS_ANDROID)
  html_source->AddResourcePath(version_ui::kAboutVersionMobileCSS,
                               IDR_VERSION_UI_MOBILE_CSS);
  html_source->AddResourcePath("images/product_logo.png", IDR_PRODUCT_LOGO);
  html_source->AddResourcePath("images/product_logo_white.png",
                               IDR_PRODUCT_LOGO_WHITE);
#endif  // defined(OS_ANDROID)
  html_source->SetDefaultResource(IDR_VERSION_UI_HTML);
  return html_source;
}

}  // namespace

VersionUI::VersionUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui->AddMessageHandler(std::make_unique<VersionHandlerChromeOS>());
#elif defined(OS_WIN)
  web_ui->AddMessageHandler(std::make_unique<VersionHandlerWindows>());
#else
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
#endif

#if !defined(OS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  WebUIDataSource::Add(profile, CreateVersionUIDataSource());
}

VersionUI::~VersionUI() {}

// static
int VersionUI::VersionProcessorVariation() {
#if defined(OS_ANDROID)
  // When building for Android, "unused" strings are removed. However, binaries
  // of both bitnesses are stripped of strings based on string analysis of one
  // bitness. Search the code for "generate_resource_allowlist" for more
  // information. Therefore, make sure both the IDS_VERSION_UI_32BIT and
  // IDS_VERSION_UI_64BIT strings are marked as always used so that they’re
  // never stripped. https://crbug.com/1119479
  IDS_VERSION_UI_32BIT;
  IDS_VERSION_UI_64BIT;
#endif  // OS_ANDROID
#if defined(OS_MAC)
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      return IDS_VERSION_UI_64BIT_INTEL;
    case base::mac::CPUType::kTranslatedIntel:
      return IDS_VERSION_UI_64BIT_TRANSLATED_INTEL;
    case base::mac::CPUType::kArm:
      return IDS_VERSION_UI_64BIT_ARM;
  }
#elif defined(ARCH_CPU_64_BITS)
  return IDS_VERSION_UI_64BIT;
#elif defined(ARCH_CPU_32_BITS)
  return IDS_VERSION_UI_32BIT;
#else
#error Update for a processor that is neither 32-bit nor 64-bit.
#endif
}

// static
void VersionUI::AddVersionDetailStrings(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedString(version_ui::kOfficial,
                                  version_info::IsOfficialBuild()
                                      ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  html_source->AddLocalizedString(version_ui::kVersionProcessorVariation,
                                  VersionProcessorVariation());

  // Data strings.
  html_source->AddString(version_ui::kVersion,
                         version_info::GetVersionNumber());
  html_source->AddString(
      version_ui::kVersionModifier,
      chrome::GetChannelName(chrome::WithExtendedStable(true)));
  html_source->AddString(version_ui::kJSEngine, "V8");
  html_source->AddString(version_ui::kJSVersion, V8_VERSION_STRING);
  html_source->AddString(
      version_ui::kCopyright,
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));
  html_source->AddString(version_ui::kCL, version_info::GetLastChange());
  html_source->AddString(version_ui::kUserAgent,
                         embedder_support::GetUserAgent());
  // Note that the executable path and profile path are retrieved asynchronously
  // and returned in VersionHandler::OnGotFilePaths. The area is initially
  // blank.
  html_source->AddString(version_ui::kExecutablePath, std::string());
  html_source->AddString(version_ui::kProfilePath, std::string());

#if defined(OS_MAC)
  html_source->AddString(version_ui::kOSType, base::mac::GetOSDisplayName());
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(version_ui::kOSType, version_info::GetOSType());
#endif  // OS_MAC

#if defined(OS_ANDROID)
  html_source->AddString(version_ui::kOSVersion,
                         AndroidAboutAppInfo::GetOsInfo());
  html_source->AddString(version_ui::kGmsVersion,
                         AndroidAboutAppInfo::GetGmsInfo());
#endif  // OS_ANDROID

#if defined(OS_WIN)
  html_source->AddString(
      version_ui::kCommandLine,
      base::AsString16(
          base::CommandLine::ForCurrentProcess()->GetCommandLineString()));
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
  html_source->AddString(version_ui::kUpdateCohortName,
                         version_utils::win::GetCohortVersionInfo());
#endif  // defined(OS_WIN)

  html_source->AddString(version_ui::kSanitizer,
                         version_info::GetSanitizerList());
}
