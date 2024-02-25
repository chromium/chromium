// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_

#include <string>
#include <vector>

#include "chrome/test/base/web_ui_mocha_browser_test.h"

class IdentityInternalsUIBrowserTest : public WebUIMochaBrowserTest {
 public:
  IdentityInternalsUIBrowserTest();

  IdentityInternalsUIBrowserTest(const IdentityInternalsUIBrowserTest&) =
      delete;
  IdentityInternalsUIBrowserTest& operator=(
      const IdentityInternalsUIBrowserTest&) = delete;

  ~IdentityInternalsUIBrowserTest() override;

 protected:
  void SetupTokenCache(int number_of_tokens);

  void SetupTokenCacheWithStoreApp();

 private:
  void AddTokenToCache(const std::string& token_id,
                       const std::string& extension_id,
                       const std::string& gaia_id,
                       const std::vector<std::string>& scopes,
                       int time_to_live);
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_BROWSERTEST_H_
