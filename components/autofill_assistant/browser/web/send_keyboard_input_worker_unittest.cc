// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/send_keyboard_input_worker.h"

#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::ElementsAre;

TEST(SendKeyboardInputWorkerTest, ConvertLowerCaseCharacter) {
  std::vector<UChar32> characters = UTF8ToUnicode("az");

  KeyEvent key_event_a =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_a.text(), "a");
  EXPECT_EQ(key_event_a.key(), "a");
  EXPECT_EQ(key_event_a.key_code(), 65);

  KeyEvent key_event_z =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(key_event_z.text(), "z");
  EXPECT_EQ(key_event_z.key(), "z");
  EXPECT_EQ(key_event_z.key_code(), 90);
}

TEST(SendKeyboardInputWorkerTest, ConvertUpperCaseCharacter) {
  std::vector<UChar32> characters = UTF8ToUnicode("AZ");

  KeyEvent key_event_a =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_a.text(), "A");
  EXPECT_EQ(key_event_a.key(), "A");
  EXPECT_EQ(key_event_a.key_code(), 65);

  KeyEvent key_event_z =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(key_event_z.text(), "Z");
  EXPECT_EQ(key_event_z.key(), "Z");
  EXPECT_EQ(key_event_z.key_code(), 90);
}

TEST(SendKeyboardInputWorkerTest, ConvertNumber) {
  std::vector<UChar32> characters = UTF8ToUnicode("09");

  KeyEvent key_event_0 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_0.text(), "0");
  EXPECT_EQ(key_event_0.key(), "0");
  EXPECT_EQ(key_event_0.key_code(), 48);

  KeyEvent key_event_9 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(key_event_9.text(), "9");
  EXPECT_EQ(key_event_9.key(), "9");
  EXPECT_EQ(key_event_9.key_code(), 57);
}

TEST(SendKeyboardInputWorkerTest, ConvertSpace) {
  std::vector<UChar32> characters = UTF8ToUnicode(" ");

  KeyEvent key_event_space =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_space.text(), " ");
  EXPECT_EQ(key_event_space.key(), " ");
  EXPECT_EQ(key_event_space.key_code(), 32);
}

TEST(SendKeyboardInputWorkerTest, ConvertControlKeys) {
  std::vector<UChar32> characters = UTF8ToUnicode("\b\r");

  KeyEvent key_event_backspace =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_backspace.text(), "\b");
  EXPECT_EQ(key_event_backspace.key(), "Backspace");
  EXPECT_THAT(key_event_backspace.command(), ElementsAre("DeleteBackward"));
  EXPECT_EQ(key_event_backspace.key_code(), 8);

  KeyEvent key_event_enter =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(key_event_enter.text(), "\r");
  EXPECT_EQ(key_event_enter.key(), "Enter");
  EXPECT_EQ(key_event_enter.key_code(), 13);
}

TEST(SendKeyboardInputWorkerTest, ConvertFixedCharacters) {
  std::vector<UChar32> characters = UTF8ToUnicode(",:");

  KeyEvent key_event_comma =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(key_event_comma.text(), ",");
  EXPECT_EQ(key_event_comma.key(), ",");
  EXPECT_EQ(key_event_comma.key_code(), 188);

  KeyEvent key_event_colon =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(key_event_colon.text(), ":");
  EXPECT_EQ(key_event_colon.key(), ":");
  EXPECT_EQ(key_event_colon.key_code(), 186);
}

TEST(SendKeyboardInputWorkerTest, ConvertMultiByteCharachters) {
  std::vector<UChar32> characters = UTF8ToUnicode("Aü万𠜎");

  KeyEvent event_0 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[0]);
  EXPECT_EQ(event_0.text(), "A");
  EXPECT_EQ(event_0.key(), "A");
  EXPECT_EQ(event_0.key_code(), 65);

  KeyEvent event_1 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[1]);
  EXPECT_EQ(event_1.text(), "ü");
  EXPECT_EQ(event_1.key(), "ü");
  EXPECT_EQ(event_1.key_code(), 0);

  KeyEvent event_2 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[2]);
  EXPECT_EQ(event_2.text(), "万");
  EXPECT_EQ(event_2.key(), "万");
  EXPECT_EQ(event_2.key_code(), 0);

  KeyEvent event_3 =
      SendKeyboardInputWorker::KeyEventFromCodepoint(characters[3]);
  EXPECT_EQ(event_3.text(), "𠜎");
  EXPECT_EQ(event_3.key(), "𠜎");
  EXPECT_EQ(event_3.key_code(), 0);
}

}  // namespace
}  // namespace autofill_assistant
