// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/preload_context.h"

#include <variant>

class Browser;
class Profile;

namespace webui {

PreloadContext::PreloadContext() = default;
PreloadContext::~PreloadContext() = default;

// static
PreloadContext PreloadContext::From(Browser* browser) {
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

const Browser* PreloadContext::GetBrowser() const {
  return IsBrowser() ? std::get<Browser*>(store_) : nullptr;
}

const Profile* PreloadContext::GetProfile() const {
  return IsProfile() ? std::get<Profile*>(store_) : nullptr;
}

bool PreloadContext::IsBrowser() const {
  return std::holds_alternative<Browser*>(store_);
}

bool PreloadContext::IsProfile() const {
  return std::holds_alternative<Profile*>(store_);
}

}  // namespace webui
