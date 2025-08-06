// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/actor_login/internal/actor_login_util.h"

#include "base/strings/utf_string_conversions.h"

namespace actor_login {

std::u16string GetSourceSiteOrAppFromUrl(const GURL& url) {
  return base::UTF8ToUTF16(url.GetWithEmptyPath().spec());
}

}  // namespace actor_login
