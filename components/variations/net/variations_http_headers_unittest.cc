// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/net/variations_http_headers.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace variations {

TEST(VariationsHttpHeadersTest, ShouldAppendHeaders) {
  struct {
    const char* url;
    bool should_append_headers;
  } cases[] = {
      {"http://google.com", false},
      {"https://google.com", true},
      {"http://www.google.com", false},
      {"https://www.google.com", true},
      {"http://m.google.com", false},
      {"https://m.google.com", true},
      {"http://google.ca", false},
      {"https://google.ca", true},
      {"http://google.co.uk", false},
      {"https://google.co.uk", true},
      {"http://google.co.uk:8080/", false},
      {"https://google.co.uk:8080/", true},
      {"http://www.google.co.uk:8080/", false},
      {"https://www.google.co.uk:8080/", true},
      {"https://google", false},

      {"http://youtube.com", false},
      {"https://youtube.com", true},
      {"http://www.youtube.com", false},
      {"https://www.youtube.com", true},
      {"http://www.youtube.ca", false},
      {"https://www.youtube.ca", true},
      {"http://www.youtube.co.uk:8080/", false},
      {"https://www.youtube.co.uk:8080/", true},
      {"https://youtube", false},

      {"https://www.yahoo.com", false},

      {"http://ad.doubleclick.net", false},
      {"https://ad.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net", true},
      {"https://a.b.c.doubleclick.net:8081", true},
      {"http://www.doubleclick.com", false},
      {"https://www.doubleclick.com", true},
      {"https://www.doubleclick.org", false},
      {"http://www.doubleclick.net.com", false},
      {"https://www.doubleclick.net.com", false},

      {"http://ad.googlesyndication.com", false},
      {"https://ad.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com", true},
      {"https://a.b.c.googlesyndication.com:8080", true},
      {"http://www.doubleclick.edu", false},
      {"http://www.googlesyndication.com.edu", false},
      {"https://www.googlesyndication.com.com", false},

      {"http://www.googleadservices.com", false},
      {"https://www.googleadservices.com", true},
      {"http://www.googleadservices.com:8080", false},
      {"https://www.googleadservices.com:8080", true},
      {"https://www.internal.googleadservices.com", true},
      {"https://www2.googleadservices.com", true},
      {"https://www.googleadservices.org", false},
      {"https://www.googleadservices.com.co.uk", false},

      {"http://WWW.ANDROID.COM", false},
      {"https://WWW.ANDROID.COM", true},
      {"http://www.android.com", false},
      {"https://www.android.com", true},
      {"http://www.doubleclick.com", false},
      {"https://www.doubleclick.com", true},
      {"http://www.doubleclick.net", false},
      {"https://www.doubleclick.net", true},
      {"http://www.ggpht.com", false},
      {"https://www.ggpht.com", true},
      {"http://www.googleadservices.com", false},
      {"https://www.googleadservices.com", true},
      {"http://www.googleapis.com", false},
      {"https://www.googleapis.com", true},
      {"http://www.googlesyndication.com", false},
      {"https://www.googlesyndication.com", true},
      {"http://www.googleusercontent.com", false},
      {"https://www.googleusercontent.com", true},
      {"http://www.googlevideo.com", false},
      {"https://www.googlevideo.com", true},
      {"http://ssl.gstatic.com", false},
      {"https://ssl.gstatic.com", true},
      {"http://www.gstatic.com", false},
      {"https://www.gstatic.com", true},
      {"http://www.ytimg.com", false},
      {"https://www.ytimg.com", true},
      {"https://wwwytimg.com", false},
      {"https://ytimg.com", false},

      {"https://www.android.org", false},
      {"https://www.doubleclick.org", false},
      {"http://www.doubleclick.net", false},
      {"https://www.doubleclick.net", true},
      {"https://www.ggpht.org", false},
      {"https://www.googleadservices.org", false},
      {"https://www.googleapis.org", false},
      {"https://www.googlesyndication.org", false},
      {"https://www.googleusercontent.org", false},
      {"https://www.googlevideo.org", false},
      {"https://ssl.gstatic.org", false},
      {"https://www.gstatic.org", false},
      {"https://www.ytimg.org", false},

      {"http://a.b.android.com", false},
      {"https://a.b.android.com", true},
      {"http://a.b.doubleclick.com", false},
      {"https://a.b.doubleclick.com", true},
      {"http://a.b.doubleclick.net", false},
      {"https://a.b.doubleclick.net", true},
      {"http://a.b.ggpht.com", false},
      {"https://a.b.ggpht.com", true},
      {"http://a.b.googleadservices.com", false},
      {"https://a.b.googleadservices.com", true},
      {"http://a.b.googleapis.com", false},
      {"https://a.b.googleapis.com", true},
      {"http://a.b.googlesyndication.com", false},
      {"https://a.b.googlesyndication.com", true},
      {"http://a.b.googleusercontent.com", false},
      {"https://a.b.googleusercontent.com", true},
      {"http://a.b.googlevideo.com", false},
      {"https://a.b.googlevideo.com", true},
      {"http://ssl.gstatic.com", false},
      {"https://ssl.gstatic.com", true},
      {"http://a.b.gstatic.com", false},
      {"https://a.b.gstatic.com", true},
      {"http://a.b.ytimg.com", false},
      {"https://a.b.ytimg.com", true},
      {"http://googleweblight.com", false},
      {"https://googleweblight.com", true},
      {"http://wwwgoogleweblight.com", false},
      {"https://www.googleweblight.com", false},
      {"https://a.b.googleweblight.com", false},

      {"http://a.b.litepages.googlezip.net", false},
      {"https://litepages.googlezip.net", false},
      {"https://a.litepages.googlezip.net", true},
      {"https://a.b.litepages.googlezip.net", true},
  };

  for (size_t i = 0; i < base::size(cases); ++i) {
    const GURL url(cases[i].url);
    EXPECT_EQ(cases[i].should_append_headers, ShouldAppendVariationHeaders(url))
        << url;
  }
}

}  // namespace variations
