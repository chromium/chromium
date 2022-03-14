// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace optimization_guide {

class PageContentAnnotationsServiceTest : public testing::Test {
 public:
  PageContentAnnotationsServiceTest() = default;
  ~PageContentAnnotationsServiceTest() override = default;

  std::string CallStringInputForPageTopicsDomain(const GURL& url) {
    return PageContentAnnotationsService::StringInputForPageTopicsDomain(url);
  }
};

TEST_F(PageContentAnnotationsServiceTest, PageTopicsDomain) {
  std::vector<std::pair<GURL, std::string>> tests = {
      {GURL("https://www.chromium.org/path?q=a"), "chromium org"},
      {GURL("https://foo-bar.com/"), "foo bar com"},
      {GURL("https://foo_bar.com/"), "foo bar com"},
      {GURL("https://cats.co.uk/"), "cats co uk"},
      {GURL("https://cats+dogs.com"), "cats dogs com"},
      {GURL("https://www.foo-bar_.baz.com"), "foo bar  baz com"},
      {GURL("https://www.foo-bar-baz.com"), "foo bar baz com"},
      {GURL("https://WwW.LOWER-CASE.com"), "lower case com"},
  };

  for (const auto& test : tests) {
    GURL url = test.first;
    std::string expected = test.second;
    std::string got = CallStringInputForPageTopicsDomain(url);

    EXPECT_EQ(expected, got) << url;
  }
}

}  // namespace optimization_guide