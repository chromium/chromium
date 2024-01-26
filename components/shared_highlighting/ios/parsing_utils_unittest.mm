// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/shared_highlighting/ios/parsing_utils.h"

#import <CoreGraphics/CoreGraphics.h>

#import "base/values.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

typedef PlatformTest ParsingUtilsTest;

namespace shared_highlighting {

// Tests the ParseRect utility function.
TEST_F(ParsingUtilsTest, ParseRect) {
  CGRect expected_rect = CGRectMake(1, 2, 3, 4);
  base::Value::Dict rect_dict;
  rect_dict.Set("x", expected_rect.origin.x);
  rect_dict.Set("y", expected_rect.origin.y);
  rect_dict.Set("width", expected_rect.size.width);
  rect_dict.Set("height", expected_rect.size.height);

  std::optional<CGRect> opt_rect = ParseRect(&rect_dict);
  ASSERT_TRUE(opt_rect.has_value());
  EXPECT_TRUE(CGRectEqualToRect(expected_rect, opt_rect.value()));

  // Invalid values.
  EXPECT_FALSE(ParseRect(nil).has_value());
  base::Value::Dict empty_dict;
  EXPECT_FALSE(ParseRect(&empty_dict).has_value());

  base::Value::Dict copied_dict = rect_dict.Clone();
  copied_dict.Remove("x");
  EXPECT_FALSE(ParseRect(&copied_dict).has_value());

  copied_dict = rect_dict.Clone();
  copied_dict.Remove("y");
  EXPECT_FALSE(ParseRect(&copied_dict).has_value());

  copied_dict = rect_dict.Clone();
  copied_dict.Remove("width");
  EXPECT_FALSE(ParseRect(&copied_dict).has_value());

  copied_dict = rect_dict.Clone();
  copied_dict.Remove("height");
  EXPECT_FALSE(ParseRect(&copied_dict).has_value());
}

// Tests for the ParseURL utility function.
TEST_F(ParsingUtilsTest, ParseURL) {
  EXPECT_FALSE(ParseURL(nil).has_value());

  std::string empty_str = "";
  EXPECT_FALSE(ParseURL(&empty_str).has_value());

  std::string invalid_url_str = "abcd";
  EXPECT_FALSE(ParseURL(&invalid_url_str).has_value());

  std::string valid_url_str = "https://www.example.com/";
  std::optional<GURL> valid_url = ParseURL(&valid_url_str);
  EXPECT_TRUE(valid_url.has_value());
  EXPECT_EQ(GURL(valid_url_str).spec(), valid_url.value().spec());
}

}  // namespace shared_highlighting
