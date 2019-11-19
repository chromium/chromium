// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/flags_ui.h"

#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui_data_source.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class FlagsUITest : public testing::Test {
 public:
  FlagsUITest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(FlagsUITest, IsDeprecatedUrl) {
  const struct {
    std::string url;
    bool is_deprecated;
  } expectations[] = {
      {"chrome://flags", false},
      {"chrome://flags/no/deprecated", false},
      {"chrome://deprecated", false},
      {"chrome://flags/deprecated", true},
      {"chrome://flags/deprecated/", true},
      {"chrome://flags//deprecated/yes?no", false},
  };

  for (const auto& expectation : expectations) {
    EXPECT_EQ(expectation.is_deprecated,
              FlagsDeprecatedUI::IsDeprecatedUrl(GURL(expectation.url)));
  }
}

TEST_F(FlagsUITest, FlagsAndDeprecatedSources) {
  std::unique_ptr<content::TestWebUIDataSource> flags_strings =
      content::TestWebUIDataSource::Create("A");
  std::unique_ptr<content::TestWebUIDataSource> deprecated_strings =
      content::TestWebUIDataSource::Create("B");
  FlagsUI::AddStrings(flags_strings->GetWebUIDataSource());
  FlagsDeprecatedUI::AddStrings(deprecated_strings->GetWebUIDataSource());
  EXPECT_EQ(flags_strings->GetLocalizedStrings()->size(),
            deprecated_strings->GetLocalizedStrings()->size());
}
