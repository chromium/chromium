// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_url_util.h"

#include <string>

#include "components/translate/core/browser/translate_download_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace translate {
namespace {

TEST(TranslateUrlUtilTest, AddHostLocaleToUrl) {
  const std::string kExampleUrl = "http://www.example.com";

  // Cache existing locale so it can be restored.
  const std::string kExistingLocale =
      TranslateDownloadManager::GetInstance()->application_locale();
  TranslateDownloadManager::GetInstance()->set_application_locale("es");
  const GURL url = AddHostLocaleToUrl(GURL(kExampleUrl));
  EXPECT_EQ(url.spec(), kExampleUrl + "/?hl=es");
  EXPECT_TRUE(url.is_valid());

  // Restore locale.
  TranslateDownloadManager::GetInstance()->set_application_locale(
      kExistingLocale);
}

}  // namespace
}  // namespace translate
