// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/keyboard_shortcut_result.h"

#include "base/test/task_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/string_matching/tokenized_string.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

using chromeos::string_matching::TokenizedString;

}  // namespace

class KeyboardShortcutResultTest : public testing::Test {};

TEST_F(KeyboardShortcutResultTest, CalculateRelevance) {
  const std::u16string query(u"minimize");
  const std::u16string target(u"Minimize window");

  const TokenizedString query_tokenized(query, TokenizedString::Mode::kWords);
  double relevance =
      KeyboardShortcutResult::CalculateRelevance(query_tokenized, target);

  EXPECT_GT(relevance, 0.5);
}

}  // namespace app_list
