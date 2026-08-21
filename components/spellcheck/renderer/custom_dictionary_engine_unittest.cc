// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/custom_dictionary_engine.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CustomDictionaryTest, HandlesEmptyWordWithInvalidSubstring) {
  CustomDictionaryEngine engine;
  std::set<std::string> custom_words;
  engine.Init(custom_words);
  EXPECT_FALSE(engine.SpellCheckWord(std::u16string(), 15, 23));
}

TEST(CustomDictionaryTest, Basic) {
  CustomDictionaryEngine engine;
  EXPECT_FALSE(engine.SpellCheckWord(u"helllo", 0, 6));
  std::set<std::string> custom_words;
  custom_words.insert("helllo");
  engine.Init(custom_words);
  EXPECT_TRUE(engine.SpellCheckWord(u"helllo", 0, 6));
}

TEST(CustomDictionaryTest, HandlesNullCharacters) {
  char16_t data[4] = {'a', 0, 'b', 'c'};
  EXPECT_FALSE(CustomDictionaryEngine().SpellCheckWord(data, 1, 1));
}

TEST(CustomDictionaryTest, InitAsync) {
  base::test::TaskEnvironment task_environment;
  CustomDictionaryEngine engine;
  EXPECT_FALSE(engine.SpellCheckWord(u"helllo", 0, 6));

  engine.InitAsync({"helllo", "world"});
  // Before tasks run, the word is not yet recognized.
  EXPECT_FALSE(engine.SpellCheckWord(u"helllo", 0, 6));

  task_environment.RunUntilIdle();
  EXPECT_TRUE(engine.SpellCheckWord(u"helllo", 0, 6));
  EXPECT_TRUE(engine.SpellCheckWord(u"world", 0, 5));
}
