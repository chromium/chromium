// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/base/l10n/l10n_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#endif

namespace {

const char kCrosUrlVersionRedirect[] = "crosUrlVersionRedirect";

}  // namespace

VersionHandlerChromeOS::VersionHandlerChromeOS() {}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {}

void VersionHandlerChromeOS::OnJavascriptDisallowed() {
  VersionHandler::OnJavascriptDisallowed();
  weak_factory_.InvalidateWeakPtrs();
}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::Value::List& args) {
  VersionHandler::HandleRequestVersionInfo(args);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Start the asynchronous load of the versions.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&VersionHandlerChromeOS::OnVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetFirmware),
      base::BindOnce(&VersionHandlerChromeOS::OnOSFirmware,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions),
      base::BindOnce(&VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions,
                     weak_factory_.GetWeakPtr()));

  const bool showSystemFlagsLink = crosapi::browser_util::IsLacrosEnabled();
#else
  const bool showSystemFlagsLink = true;
#endif

  FireWebUIListener("return-lacros-primary", base::Value(showSystemFlagsLink));
}

void VersionHandlerChromeOS::RegisterMessages() {
  VersionHandler::RegisterMessages();

  web_ui()->RegisterMessageCallback(
      kCrosUrlVersionRedirect,
      base::BindRepeating(&VersionHandlerChromeOS::HandleCrosUrlVersionRedirect,
                          base::Unretained(this)));
}

void VersionHandlerChromeOS::HandleCrosUrlVersionRedirect(
    const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_url_handling::NavigateInAsh(GURL(chrome::kOsUIVersionURL));
#else
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUIVersionURL),
      /*path_behavior=*/NavigateParams::RESPECT);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void VersionHandlerChromeOS::OnVersion(
    const absl::optional<std::string>& version) {
  FireWebUIListener("return-os-version",
                    base::Value(version.value_or("0.0.0.0")));
}

void VersionHandlerChromeOS::OnOSFirmware(const std::string& version) {
  FireWebUIListener("return-os-firmware-version", base::Value(version));
}

void VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions(
    const std::string& version) {
  FireWebUIListener("return-arc-and-arc-android-sdk-versions",
                    base::Value(version));
}

// static
std::string VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions() {
  std::string arc_version = chromeos::version_loader::GetArcVersion();
  absl::optional<std::string> arc_android_sdk_version =
      chromeos::version_loader::GetArcAndroidSdkVersion();
  if (!arc_android_sdk_version.has_value()) {
    arc_android_sdk_version = base::UTF16ToUTF8(
        l10n_util::GetStringUTF16(IDS_ARC_SDK_VERSION_UNKNOWN));
  }
  std::string sdk_label =
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ARC_SDK_VERSION_LABEL));
  std::string labeled_version =
      base::StringPrintf("%s %s %s", arc_version.c_str(), sdk_label.c_str(),
                         arc_android_sdk_version->c_str());
  return labeled_version;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
