// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webapps/chrome_webapps_client.h"

#include "chrome/browser/ssl/security_state_tab_helper.h"

namespace webapps {

// static
ChromeWebappsClient* ChromeWebappsClient::GetInstance() {
  static base::NoDestructor<ChromeWebappsClient> instance;
  return instance.get();
}

security_state::SecurityLevel
ChromeWebappsClient::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
  return SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityLevel();
}

}  // namespace webapps
