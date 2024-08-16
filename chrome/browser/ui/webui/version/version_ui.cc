// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/version/version_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/debugging_buildflags.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version/version_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/grit/components_scaled_resources.h"
#include "components/grit/version_ui_resources.h"
#include "components/grit/version_ui_resources_map.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/service/variations_service.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#else
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/ui/webui/version/version_handler_win.h"
#include "chrome/browser/ui/webui/version/version_util_win.h"
#endif

using content::WebUIDataSource;

namespace {

void CreateAndAddVersionUIDataSource(Profile* profile) {
  WebUIDataSource* html_source =
      WebUIDataSource::CreateAndAdd(profile, chrome::kChromeUIVersionHost);
  // These localized strings are used to label version details.
  static constexpr webui::LocalizedString kStrings[] = {
    {version_ui::kTitle, IDS_VERSION_UI_TITLE},
    {version_ui::kLogoAltText, IDS_SHORT_PRODUCT_LOGO_ALT_TEXT},
    {version_ui::kApplicationLabel, IDS_PRODUCT_NAME},
    {version_ui::kCompany, IDS_ABOUT_VERSION_COMPANY_NAME},
    {version_ui::kCopyLabel, IDS_VERSION_UI_COPY_LABEL},
    {version_ui::kCopyNotice, IDS_VERSION_UI_COPY_NOTICE},
    {version_ui::kRevision, IDS_VERSION_UI_REVISION},
    {version_ui::kUserAgentName, IDS_VERSION_UI_USER_AGENT},
    {version_ui::kCommandLineName, IDS_VERSION_UI_COMMAND_LINE},
    {version_ui::kExecutablePathName, IDS_VERSION_UI_EXECUTABLE_PATH},
    {version_ui::kProfilePathName, IDS_VERSION_UI_PROFILE_PATH},
    {version_ui::kVariationsName, IDS_VERSION_UI_VARIATIONS},
    {version_ui::kVariationsCmdName, IDS_VERSION_UI_VARIATIONS_CMD},
    {version_ui::kVariationsSeedName, IDS_VERSION_UI_VARIATIONS_SEED_NAME},
#if BUILDFLAG(IS_CHROMEOS)
    {version_ui::kARC, IDS_ARC_LABEL},
    {version_ui::kPlatform, IDS_PLATFORM_LABEL},
    {version_ui::kCustomizationId, IDS_VERSION_UI_CUSTOMIZATION_ID},
    {version_ui::kFirmwareVersion, IDS_VERSION_UI_FIRMWARE_VERSION},
    {version_ui::kOsVersionHeaderText1, IDS_VERSION_UI_OS_TEXT1_LABEL},
    {version_ui::kOsVersionHeaderText2, IDS_VERSION_UI_OS_TEXT2_LABEL},
#endif  // BUILDFLAG(IS_CHROMEOS)
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    {version_ui::kOSName, IDS_VERSION_UI_OS},
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_ANDROID)
    {version_ui::kGmsName, IDS_VERSION_UI_GMS},
#endif  // BUILDFLAG(IS_ANDROID)
  };
  html_source->AddLocalizedStrings(kStrings);

#if BUILDFLAG(IS_CHROMEOS)
  auto os_link = l10n_util::GetStringUTF16(IDS_VERSION_UI_OS_LINK);
  html_source->AddString(version_ui::kOsVersionHeaderLink, os_link);
#endif  // BUILDFLAG(IS_CHROMEOS)

  VersionUI::AddVersionDetailStrings(html_source);

  html_source->AddResourcePaths(
      base::make_span(kVersionUiResources, kVersionUiResourcesSize));
  html_source->UseStringsJs();

#if BUILDFLAG(IS_ANDROID)
  html_source->AddResourcePath("images/product_logo.png", IDR_PRODUCT_LOGO);
  html_source->AddResourcePath("images/product_logo_white.png",
                               IDR_PRODUCT_LOGO_WHITE);
#endif  // BUILDFLAG(IS_ANDROID)
  html_source->SetDefaultResource(IDR_VERSION_UI_ABOUT_VERSION_HTML);
}

std::string GetProductModifier() {
  std::vector<std::string> modifier_parts;
  if (std::string channel_name =
          chrome::GetChannelName(chrome::WithExtendedStable(true));
      !channel_name.empty()) {
    modifier_parts.push_back(std::move(channel_name));
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  modifier_parts.emplace_back("lacros");
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  modifier_parts.emplace_back("dcheck");
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)
  return base::JoinString(modifier_parts, "-");
}

}  // namespace

VersionUI::VersionUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if BUILDFLAG(IS_CHROMEOS)
  web_ui->AddMessageHandler(std::make_unique<VersionHandlerChromeOS>());
#elif BUILDFLAG(IS_WIN)
  web_ui->AddMessageHandler(std::make_unique<VersionHandlerWindows>());
#else
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Set up the chrome://theme/ source.
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  CreateAndAddVersionUIDataSource(profile);
}

VersionUI::~VersionUI() {}

// static
int VersionUI::VersionProcessorVariation() {
#if BUILDFLAG(IS_ANDROID)
  // When building for Android, "unused" strings are removed. However, binaries
  // of both bitnesses are stripped of strings based on string analysis of one
  // bitness. Search the code for "generate_resource_allowlist" for more
  // information. Therefore, make sure both the IDS_VERSION_UI_32BIT and
  // IDS_VERSION_UI_64BIT strings are marked as always used so that they’re
  // never stripped. https://crbug.com/1119479
  IDS_VERSION_UI_32BIT;
  IDS_VERSION_UI_64BIT;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_MAC)
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      return IDS_VERSION_UI_64BIT_INTEL;
    case base::mac::CPUType::kTranslatedIntel:
      return IDS_VERSION_UI_64BIT_TRANSLATED_INTEL;
    case base::mac::CPUType::kArm:
      return IDS_VERSION_UI_64BIT_ARM;
  }
#elif BUILDFLAG(IS_WIN)
#if defined(ARCH_CPU_ARM64)
  return IDS_VERSION_UI_64BIT_ARM;
#else
  bool emulated = base::win::OSInfo::IsRunningEmulatedOnArm64();
#if defined(ARCH_CPU_X86)
  if (emulated) {
    return IDS_VERSION_UI_32BIT_TRANSLATED_INTEL;
  }
  return IDS_VERSION_UI_32BIT;
#else   // defined(ARCH_CPU_X86)
  if (emulated) {
    return IDS_VERSION_UI_64BIT_TRANSLATED_INTEL;
  }
  return IDS_VERSION_UI_64BIT;
#endif  // defined(ARCH_CPU_X86)
#endif  // defined(ARCH_CPU_ARM64)
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
                         std::string(version_info::GetVersionNumber()));

  html_source->AddString(version_ui::kVersionModifier, GetProductModifier());

  html_source->AddString(version_ui::kJSEngine, "V8");
  html_source->AddString(version_ui::kJSVersion, V8_VERSION_STRING);
  html_source->AddString(
      version_ui::kCopyright,
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_COPYRIGHT),
          base::Time::Now()));
  html_source->AddString(version_ui::kCL,
                         std::string(version_info::GetLastChange()));
  html_source->AddString(version_ui::kUserAgent,
                         embedder_support::GetUserAgent());
  // Note that the executable path and profile path are retrieved asynchronously
  // and returned in VersionHandler::OnGotFilePaths. The area is initially
  // blank.
  html_source->AddString(version_ui::kExecutablePath, std::string());
  html_source->AddString(version_ui::kProfilePath, std::string());

#if BUILDFLAG(IS_MAC)
  html_source->AddString(version_ui::kOSType, base::mac::GetOSDisplayName());
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
  html_source->AddString(version_ui::kOSType,
                         std::string(version_info::GetOSType()));
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
  std::string os_info = AndroidAboutAppInfo::GetOsInfo();
  os_info += "; " + base::NumberToString(
                        base::android::BuildInfo::GetInstance()->sdk_int());
  std::string code_name(base::android::BuildInfo::GetInstance()->codename());
  os_info += "; " + code_name;
  html_source->AddString(version_ui::kOSVersion, os_info);
  html_source->AddString(
      version_ui::kTargetSdkVersion,
      base::NumberToString(
          base::android::BuildInfo::GetInstance()->target_sdk_version()));
  html_source->AddString(version_ui::kTargetsU,
                         AndroidAboutAppInfo::GetTargetsUInfo());
  html_source->AddString(version_ui::kGmsVersion,
                         AndroidAboutAppInfo::GetGmsInfo());
  html_source->AddString(
      version_ui::kVersionCode,
      base::android::BuildInfo::GetInstance()->package_version_code());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  html_source->AddString(
      version_ui::kCommandLine,
      base::AsString16(
          base::CommandLine::ForCurrentProcess()->GetCommandLineString()));
#else
  std::string command_line;
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (auto iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  html_source->AddString(version_ui::kCommandLine, command_line);
#endif

#if BUILDFLAG(IS_MAC)
  html_source->AddString("linker", CHROMIUM_LINKER_NAME);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  html_source->AddString(version_ui::kUpdateCohortName,
                         version_utils::win::GetCohortVersionInfo());
#endif  // BUILDFLAG(IS_WIN)

  html_source->AddString(
      version_ui::kVariationsSeed,
      g_browser_process->variations_service()
          ? version_ui::SeedTypeToUiString(
                g_browser_process->variations_service()->GetSeedType())
          : std::string());

  html_source->AddString(version_ui::kSanitizer,
                         std::string(version_info::GetSanitizerList()));
}

#if !BUILDFLAG(IS_ANDROID)
// static
std::u16string VersionUI::GetAnnotatedVersionStringForUi() {
  return l10n_util::GetStringFUTF16(
      IDS_SETTINGS_ABOUT_PAGE_BROWSER_VERSION,
      base::UTF8ToUTF16(version_info::GetVersionNumber()),
      l10n_util::GetStringUTF16(version_info::IsOfficialBuild()
                                    ? IDS_VERSION_UI_OFFICIAL
                                    : IDS_VERSION_UI_UNOFFICIAL),
      base::UTF8ToUTF16(GetProductModifier()),
      l10n_util::GetStringUTF16(VersionUI::VersionProcessorVariation()));
}
#endif  // !BUILDFLAG(IS_ANDROID)
