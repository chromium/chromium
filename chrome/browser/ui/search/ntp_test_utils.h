// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_NTP_TEST_UTILS_H_
#define CHROME_BROWSER_UI_SEARCH_NTP_TEST_UTILS_H_

#include <string>

class GURL;
class Profile;

namespace ntp_test_utils {

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url);

// Get the URL that WebContents->GetVisibleURL() will return after navigating to
// chrome://newtab/.  While this should typically be chrome://newtab/, in a test
// environment where there is no network connection, it may be
// chrome://new-tab-page-third-party.
GURL GetFinalNtpUrl(Profile* profile);

}  // namespace ntp_test_utils

#endif  // CHROME_BROWSER_UI_SEARCH_NTP_TEST_UTILS_H_
