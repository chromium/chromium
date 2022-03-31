// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version/version_handler_chromeos.h"

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/webui_url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
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
      base::BindOnce(&chromeos::version_loader::GetARCVersion),
      base::BindOnce(&VersionHandlerChromeOS::OnARCVersion,
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
      GURL(chrome::kChromeUIVersionURL));
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void VersionHandlerChromeOS::OnVersion(const std::string& version) {
  FireWebUIListener("return-os-version", base::Value(version));
}

void VersionHandlerChromeOS::OnOSFirmware(const std::string& version) {
  FireWebUIListener("return-os-firmware-version", base::Value(version));
}

void VersionHandlerChromeOS::OnARCVersion(const std::string& version) {
  FireWebUIListener("return-arc-version", base::Value(version));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
