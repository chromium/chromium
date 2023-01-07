// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_TEST_UTIL_H_

#include "base/memory/scoped_refptr.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// Returns a request object with |web_contents|. Other request options are set
// to some default value and potentially nonsensical.
scoped_refptr<PasswordProtectionRequestContent> CreateDummyRequest(
    content::WebContents* web_contents);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_TEST_UTIL_H_
