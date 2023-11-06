// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/credential_utils.h"

#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"

namespace password_manager {

bool IsValidPasswordURL(const GURL& url) {
  return url.is_valid() &&
         (url.SchemeIsHTTPOrHTTPS() ||
          password_manager::IsValidAndroidFacetURI(url.spec()));
}

}  // namespace password_manager