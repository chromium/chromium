// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_isolation_delegate.h"

#include "base/no_destructor.h"

namespace web_app {

namespace {

WebAppIsolationDelegate::FactoryCallback& GetFactory() {
  static base::NoDestructor<WebAppIsolationDelegate::FactoryCallback> factory;
  return *factory;
}

}  // namespace

void WebAppIsolationDelegate::RegisterFactory(
    base::PassKey<BrowserProcessImpl, TestingBrowserProcess> pass_key,
    FactoryCallback factory) {
  GetFactory() = std::move(factory);
}

std::unique_ptr<WebAppIsolationDelegate> WebAppIsolationDelegate::Create(
    base::PassKey<WebAppProvider> pass_key,
    Profile* profile,
    ui::UnownedUserDataHost& unowned_user_data_host) {
  CHECK(!GetFactory().is_null());
  return GetFactory().Run(profile, unowned_user_data_host);
}

WebAppIsolationDelegate::WebAppIsolationDelegate() = default;

WebAppIsolationDelegate::~WebAppIsolationDelegate() = default;

}  // namespace web_app
