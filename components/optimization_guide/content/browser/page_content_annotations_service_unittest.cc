// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotations_service.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PageContentAnnotationsServiceTest : public testing::Test {
 public:
  PageContentAnnotationsServiceTest() = default;
  ~PageContentAnnotationsServiceTest() override = default;

  std::string CallStringInputForPageTopicsHost(const std::string& host) {
    return PageContentAnnotationsService::StringInputForPageTopicsHost(host);
  }
};

TEST_F(PageContentAnnotationsServiceTest, PageTopicsHost) {
  std::vector<std::pair<std::string, std::string>> tests = {
      {"www.chromium.org", "chromium org"},
      {"foo-bar.com", "foo bar com"},
      {"foo_bar.com", "foo bar com"},
      {"cats.co.uk", "cats co uk"},
      {"cats+dogs.com", "cats dogs com"},
      {"www.foo-bar_.baz.com", "foo bar  baz com"},
      {"www.foo-bar-baz.com", "foo bar baz com"},
      {"WwW.LOWER-CASE.com", "lower case com"},
  };

  for (const auto& test : tests) {
    std::string host = test.first;
    std::string expected = test.second;
    std::string got = CallStringInputForPageTopicsHost(host);

    EXPECT_EQ(expected, got) << host;
  }
}

}  // namespace optimization_guide