// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/version/version_loader.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kCrosUrlVersionRedirect[] = "crosUrlVersionRedirect";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
std::string GetOsVersion() {
  return chromeos::BrowserParamsProxy::Get()->AshChromeVersion().value_or(
      "0.0.0.0");
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

  // Start the asynchronous load of the versions.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetOsVersion),
      base::BindOnce(&VersionHandlerChromeOS::OnOsVersion,
                     weak_factory_.GetWeakPtr()));
#endif
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&VersionHandlerChromeOS::OnPlatformVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetFirmware),
      base::BindOnce(&VersionHandlerChromeOS::OnFirmwareVersion,
                     weak_factory_.GetWeakPtr()));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions),
      base::BindOnce(&VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions,
                     weak_factory_.GetWeakPtr()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool is_lacros_enabled = crosapi::browser_util::IsLacrosEnabled();
  FireWebUIListener("return-lacros-enabled", base::Value(is_lacros_enabled));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void VersionHandlerChromeOS::RegisterMessages() {
  VersionHandler::RegisterMessages();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      kCrosUrlVersionRedirect,
      base::BindRepeating(&VersionHandlerChromeOS::HandleCrosUrlVersionRedirect,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void VersionHandlerChromeOS::HandleCrosUrlVersionRedirect(
    const base::Value::List& args) {
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUIVersionURL),
      /*path_behavior=*/NavigateParams::RESPECT);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void VersionHandlerChromeOS::OnOsVersion(const std::string& version) {
  FireWebUIListener("return-os-version", base::Value(version));
}
#endif

void VersionHandlerChromeOS::OnPlatformVersion(
    const std::optional<std::string>& version) {
  FireWebUIListener("return-platform-version",
                    base::Value(version.value_or("0.0.0.0")));
}

void VersionHandlerChromeOS::OnFirmwareVersion(const std::string& version) {
  FireWebUIListener("return-firmware-version", base::Value(version));
}

void VersionHandlerChromeOS::OnArcAndArcAndroidSdkVersions(
    const std::string& version) {
  FireWebUIListener("return-arc-and-arc-android-sdk-versions",
                    base::Value(version));
}

// static
std::string VersionHandlerChromeOS::GetArcAndArcAndroidSdkVersions() {
  std::string arc_version = chromeos::version_loader::GetArcVersion();
  std::optional<std::string> arc_android_sdk_version =
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
