// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_data.h"

#include <string_view>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(TemplateURLDataTest, Trim) {
  TemplateURLData data(
      u" shortname ", u" keyword ", "https://cs.chromium.org",
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), std::string_view(),
      std::string_view(), std::string_view(), {}, std::string_view(),
      std::string_view(), std::u16string_view(), base::Value::List(), false,
      false, 0);

  EXPECT_EQ(u"shortname", data.short_name());
  EXPECT_EQ(u"keyword", data.keyword());

  data.SetShortName(u" othershortname ");
  data.SetKeyword(u" otherkeyword ");

  EXPECT_EQ(u"othershortname", data.short_name());
  EXPECT_EQ(u"otherkeyword", data.keyword());
}
