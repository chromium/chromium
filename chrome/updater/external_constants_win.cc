// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants.h"

#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_impl.h"
#include "chrome/updater/win/constants.h"
#include "url/gurl.h"

namespace updater {

std::vector<GURL> DevOverrideProvider::UpdateURL() const {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, UPDATE_DEV_KEY, KEY_READ) == ERROR_SUCCESS) {
    base::string16 url;
    if (key.ReadValue(base::UTF8ToUTF16(kDevOverrideKeyUrl).c_str(), &url) ==
        ERROR_SUCCESS)
      return {GURL(url)};
  }
  return next_provider_->UpdateURL();
}

bool DevOverrideProvider::UseCUP() const {
  base::win::RegKey key;
  if (key.Open(HKEY_CURRENT_USER, UPDATE_DEV_KEY, KEY_READ) == ERROR_SUCCESS) {
    DWORD use_cup = 0;
    if (key.ReadValueDW(base::UTF8ToUTF16(kDevOverrideKeyUseCUP).c_str(),
                        &use_cup) == ERROR_SUCCESS) {
      if (use_cup == 0)
        return false;
      else if (use_cup == 1)
        return true;
    }
  }
  return next_provider_->UseCUP();
}

}  // namespace updater
