// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"

#include "base/lazy_instance.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"
#include "extensions/buildflags/buildflags.h"
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/updater/extension_updater_switches.h"
#endif

namespace {

base::LazyInstance<ChromeUpdateQueryParamsDelegate>::DestructorAtExit
    g_chrome_update_query_params_delegate = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeUpdateQueryParamsDelegate::ChromeUpdateQueryParamsDelegate() {
}

ChromeUpdateQueryParamsDelegate::~ChromeUpdateQueryParamsDelegate() {
}

// static
ChromeUpdateQueryParamsDelegate*
ChromeUpdateQueryParamsDelegate::GetInstance() {
  return g_chrome_update_query_params_delegate.Pointer();
}

std::string ChromeUpdateQueryParamsDelegate::GetExtraParams() {
  std::string channel_name;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  channel_name = extensions::GetChannelForExtensionUpdates();
#else
  channel_name = chrome::GetChannelName(chrome::WithExtendedStable(true));
#endif

  return base::StrCat({"&prodchannel=", channel_name, "&prodversion=",
                       version_info::GetVersionNumber(), "&lang=", GetLang()});
}

// static
const char* ChromeUpdateQueryParamsDelegate::GetLang() {
  return g_browser_process->GetApplicationLocale().c_str();
}
