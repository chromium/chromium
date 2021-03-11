// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace javascript_dialogs {

TEST(AppModalDialogManagerTest, GetTitle) {
  struct Case {
    const char* parent_url;
    const char* alerting_url;
    const char* expected;
    const char* expected_android;
  } cases[] = {
      // Standard main frame alert.
      {"http://foo.com/", "http://foo.com/", "foo.com says", "foo.com says"},

      // Subframe alert from the same origin.
      {"http://foo.com/1", "http://foo.com/2", "foo.com says", "foo.com says"},
      // Subframe alert from a different origin.
      {"http://foo.com/", "http://bar.com/", "An embedded page at bar.com says",
       "An embedded page at bar.com says"},

      // file:
      // - main frame:
      {"file:///path/to/page.html", "file:///path/to/page.html",
       "This page says", "This page says"},
      // - subframe:
      {"http://foo.com/", "file:///path/to/page.html",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // ftp:
      // - main frame:
      {"ftp://foo.com/path/to/page.html", "ftp://foo.com/path/to/page.html",
       "foo.com says", "ftp://foo.com says"},
      // - subframe:
      {"http://foo.com/", "ftp://foo.com/path/to/page.html",
       "An embedded page at foo.com says",
       "An embedded page at ftp://foo.com says"},

      // data:
      // - main frame:
      {"data:blahblah", "data:blahblah", "This page says", "This page says"},
      // - subframe:
      {"http://foo.com/", "data:blahblah", "An embedded page on this page says",
       "An embedded page on this page says"},

      // javascript:
      // - main frame:
      {"javascript:abc", "javascript:abc", "This page says", "This page says"},
      // - subframe:
      {"http://foo.com/", "javascript:abc",
       "An embedded page on this page says",
       "An embedded page on this page says"},

      // about:
      // - main frame:
      {"about:blank", "about:blank", "This page says", "This page says"},
      // - subframe:
      {"http://foo.com/", "about:blank", "An embedded page on this page says",
       "An embedded page on this page says"},

      // blob:
      // - main frame:
      {"blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "foo.com says", "foo.com says"},
      // - subframe:
      {"http://bar.com/",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "An embedded page at foo.com says", "An embedded page at foo.com says"},

      // filesystem:
      // - main frame:
      {"filesystem:http://foo.com/bar.html",
       "filesystem:http://foo.com/bar.html", "foo.com says", "foo.com says"},
      // - subframe:
      {"http://bar.com/", "filesystem:http://foo.com/bar.html",
       "An embedded page at foo.com says", "An embedded page at foo.com says"},
  };

  for (const auto& test_case : cases) {
    std::u16string result = AppModalDialogManager::GetTitleImpl(
        GURL(test_case.parent_url), GURL(test_case.alerting_url));
#if defined(OS_ANDROID)
    EXPECT_EQ(test_case.expected_android, base::UTF16ToUTF8(result));
#else
    EXPECT_EQ(test_case.expected, base::UTF16ToUTF8(result));
#endif
  }
}

}  // namespace javascript_dialogs
