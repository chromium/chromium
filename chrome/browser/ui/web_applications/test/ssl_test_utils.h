// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_SSL_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_SSL_TEST_UTILS_H_

class Browser;

namespace net {
class SSLInfo;
}

namespace web_app {

// Checks that the active tab's authentication state indicates insecure content.
void CheckMixedContentLoaded(Browser* browser);

// Checks that the active tab's authentication state indicates only secure
// content is shown.
void CheckMixedContentFailedToLoad(Browser* browser);

void CreateFakeSslInfoCertificate(net::SSLInfo* ssl_info);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_TEST_SSL_TEST_UTILS_H_
