// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/ntp_test_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"

namespace ntp_test_utils {

void SetUserSelectedDefaultSearchProvider(Profile* profile,
                                          const std::string& base_url,
                                          const std::string& ntp_url) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(base_url));
  data.SetKeyword(base::UTF8ToUTF16(base_url));
  data.SetURL(base_url + "url?bar={searchTerms}");
  data.new_tab_url = ntp_url;

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
}

GURL GetFinalNtpUrl(Profile* profile) {
  if (search::GetNewTabPageURL(profile) ==
      GURL(chrome::kChromeUINewTabPageURL)) {
    // If chrome://newtab/ already maps to the local/WebUI NTP, then that will
    // load correctly, even without network.  The URL associated with the
    // WebContents will stay chrome://newtab/.
    return GURL(chrome::kChromeUINewTabURL);
  }
  // If chrome://newtab/ maps to a remote URL, then it will fail to load in a
  // browser_test environment.  In this case, we will get redirected to the
  // 3P WebUI NTP, which changes the URL associated with the WebContents.
  return GURL(chrome::kChromeUINewTabPageThirdPartyURL);
}

}  // namespace ntp_test_utils
