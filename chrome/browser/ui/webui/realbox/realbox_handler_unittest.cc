// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/realbox/realbox_handler.h"

#include <string>

#include <gtest/gtest.h>
#include "base/check.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/vector_icon_types.h"

class RealboxHandlerIconTest : public testing::TestWithParam<bool> {
 public:
  RealboxHandlerIconTest() = default;
};

INSTANTIATE_TEST_SUITE_P(, RealboxHandlerIconTest, testing::Bool());

// Tests that all Omnibox vector icons map to an equivalent SVG for use in the
// NTP Realbox.
TEST_P(RealboxHandlerIconTest, VectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    const bool is_bookmark = GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        RealboxHandler::AutocompleteMatchVectorIconToResourceName(vector_icon);
    if (vector_icon.name == omnibox::kBlankIcon.name) {
      // An empty resource name is effectively a blank icon.
      ASSERT_TRUE(svg_name.empty());
    } else if (vector_icon.name == omnibox::kPedalIcon.name) {
      // Pedals are not supported in the NTP Realbox.
      ASSERT_TRUE(svg_name.empty());
    } else if (is_bookmark) {
      ASSERT_EQ("chrome://resources/images/icon_bookmark.svg", svg_name);
    } else {
      ASSERT_FALSE(svg_name.empty());
    }
  }
}
