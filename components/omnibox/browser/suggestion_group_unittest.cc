// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/suggestion_group.h"

#include "testing/gtest/include/gtest/gtest.h"

// Ensures accessing unset fields is safe and returns the default value. See:
// https://developers.google.com/protocol-buffers/docs/reference/cpp-generated
TEST(SuggestionGroupTest, UnsetFieldDefaultValue) {
  omnibox::GroupConfig group_config;
  ASSERT_EQ("", group_config.header_text());
  ASSERT_EQ(omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
            group_config.side_type());
  ASSERT_EQ(omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL,
            group_config.render_type());
  ASSERT_EQ(omnibox::GroupConfig_Visibility_DEFAULT_VISIBLE,
            group_config.visibility());
  ASSERT_EQ(0U, group_config.why_this_result_reason());
}
