// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/update_client/chrome_update_query_params_delegate.h"

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/version_info.h"

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
  return base::StringPrintf(
      "&prodchannel=%s&prodversion=%s&lang=%s",
      chrome::GetChannelName(chrome::WithExtendedStable(true)).c_str(),
      version_info::GetVersionNumber().c_str(), GetLang());
}

// static
const char* ChromeUpdateQueryParamsDelegate::GetLang() {
  return g_browser_process->GetApplicationLocale().c_str();
}
