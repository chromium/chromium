// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"

#include <memory>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/top_chrome_webui_profile_impl.h"

namespace webui {

namespace {

std::unique_ptr<TopChromeWebUIProfileImpl>& GetTopChromeWebUIProfileInstance() {
  static base::NoDestructor<std::unique_ptr<TopChromeWebUIProfileImpl>> profile;
  return *profile;
}

}  // namespace

void CreateTopChromeWebUIProfile() {
  std::unique_ptr<TopChromeWebUIProfileImpl>& profile =
      GetTopChromeWebUIProfileInstance();
  CHECK(!profile);
  profile = std::make_unique<TopChromeWebUIProfileImpl>();
}

void DestroyTopChromeWebUIProfile() {
  GetTopChromeWebUIProfileInstance().reset();
}

Profile* GetTopChromeWebUIProfile() {
  return GetTopChromeWebUIProfileInstance().get();
}

ScopedTopChromeWebUIProfileForTesting::ScopedTopChromeWebUIProfileForTesting() {
  CreateTopChromeWebUIProfile();
}

ScopedTopChromeWebUIProfileForTesting::
    ~ScopedTopChromeWebUIProfileForTesting() {
  DestroyTopChromeWebUIProfile();
}

}  // namespace webui
