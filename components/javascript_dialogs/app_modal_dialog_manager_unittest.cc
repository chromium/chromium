// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace javascript_dialogs {

TEST(AppModalDialogManagerTest, GetTitle) {
  struct Case {
    // The name of the test case.
    const char* case_name;

    // The URL of the main frame of the page.
    const char* main_frame_url;

    // Whether the main frame is alerting.
    bool is_main_frame;

    // If `is_main_frame` is false, the URL of the alerting frame of the page.
    const char* alerting_frame_url;

    // The expected title for the alert.
    const char* expected;
  } cases[] = {
      // Standard main frame alert.
      {"standard", "http://foo.com/", true, "", "foo.com says"},

      // Subframe alert from the same origin.
      {"subframe same origin", "http://foo.com/1", false, "http://foo.com/2",
       "foo.com says"},
      // Subframe alert from a different origin.
      {"subframe different origin", "http://foo.com/", false, "http://bar.com/",
       "An embedded page at bar.com says"},

      // file:
      // - main frame:
      {"file main frame", "file:///path/to/page.html", true, "",
       "This page says"},
      // - subframe:
      {"file subframe", "http://foo.com/", false, "file:///path/to/page.html",
       "An embedded page on this page says"},

      // data:
      // /!\ NOTE that this is for data URLs entered directly in the omnibox.
      // For pages that generate frames with data URLs, see the browsertest.
      // - main frame:
      {"data main frame", "data:blahblah", true, "", "This page says"},
      // - subframe:
      {"data subframe", "http://foo.com/", false, "data:blahblah",
       "An embedded page on this page says"},

      // javascript:
      // /!\ NOTE that this is for javascript URLs entered directly in the
      // omnibox. For pages that generate frames with javascript URLs, see the
      // browsertest.
      // - main frame:
      {"javascript main frame", "javascript:abc", true, "", "This page says"},
      // - subframe:
      {"javascript subframe", "http://foo.com/", false, "javascript:abc",
       "An embedded page on this page says"},

      // about:
      // /!\ NOTE that this is for about:blank URLs entered directly in the
      // omnibox. For pages that generate frames with about:blank URLs, see the
      // browsertest.
      // - main frame:
      {"about main frame", "about:blank", true, "", "This page says"},
      // - subframe:
      {"about subframe", "http://foo.com/", false, "about:blank",
       "An embedded page on this page says"},

      // blob:
      // - main frame:
      {"blob main frame",
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666", true, "",
       "foo.com says"},
      // - subframe:
      {"blob subframe", "http://bar.com/", false,
       "blob:http://foo.com/66666666-6666-6666-6666-666666666666",
       "An embedded page at foo.com says"},

      // filesystem:
      // - main frame:
      {"filesystem main frame", "filesystem:http://foo.com/bar.html", true, "",
       "foo.com says"},
      // - subframe:
      {"filesystem subframe", "http://bar.com/", false,
       "filesystem:http://foo.com/bar.html",
       "An embedded page at foo.com says"},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.case_name);
    url::Origin main_frame_origin =
        url::Origin::Create(GURL(test_case.main_frame_url));
    url::Origin alerting_frame_origin =
        test_case.is_main_frame
            ? main_frame_origin
            : url::Origin::Create(GURL(test_case.alerting_frame_url));
    std::u16string result = AppModalDialogManager::GetSiteFrameTitle(
        main_frame_origin, alerting_frame_origin);
    EXPECT_EQ(test_case.expected, base::UTF16ToUTF8(result));
  }
}

}  // namespace javascript_dialogs
