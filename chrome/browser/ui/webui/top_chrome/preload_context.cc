// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/preload_context.h"

#include <variant>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

class BrowserWindowInterface;
class Profile;

namespace webui {

PreloadContext::PreloadContext() = default;
PreloadContext::~PreloadContext() = default;

// static
PreloadContext PreloadContext::From(BrowserWindowInterface* browser) {
  PreloadContext context;
  context.store_ = browser;
  return context;
}

// static
PreloadContext PreloadContext::From(Profile* profile) {
  PreloadContext context;
  context.store_ = profile;
  return context;
}

BrowserWindowInterface* PreloadContext::GetBrowser() {
  return IsBrowser() ? std::get<BrowserWindowInterface*>(store_) : nullptr;
}

const BrowserWindowInterface* PreloadContext::GetBrowser() const {
  return IsBrowser() ? std::get<BrowserWindowInterface*>(store_) : nullptr;
}

Profile* PreloadContext::GetProfile() {
  return IsProfile() ? std::get<Profile*>(store_) : nullptr;
}

const Profile* PreloadContext::GetProfile() const {
  return IsProfile() ? std::get<Profile*>(store_) : nullptr;
}

bool PreloadContext::IsBrowser() const {
  return std::holds_alternative<BrowserWindowInterface*>(store_);
}

bool PreloadContext::IsProfile() const {
  return std::holds_alternative<Profile*>(store_);
}

}  // namespace webui
