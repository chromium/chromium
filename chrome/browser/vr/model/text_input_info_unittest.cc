// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/model/text_input_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace vr {

class TextInputInfoTest : public testing::Test {
 protected:
  TextInputInfo Text(const std::string& text,
                     int selection_start,
                     int selection_end,
                     int composition_start,
                     int composition_end) {
    return TextInputInfo(base::UTF8ToUTF16(text), selection_start,
                         selection_end, composition_start, composition_end);
  }

  TextInputInfo Text(const std::string& text,
                     int selection_start,
                     int selection_end) {
    return Text(text, selection_start, selection_end, -1, -1);
  }
};

TEST(TextInputInfo, Clamping) {
  // Out of bounds indices.
  auto info = TextInputInfo(u"hi", 4, 4, 4, 4);
  auto info_expected = TextInputInfo(u"hi", 2, 2, -1, -1);
  EXPECT_EQ(info, info_expected);

  // Invalid indices.
  info = TextInputInfo(u"hi", 4, 2, 2, 1);
  info_expected = TextInputInfo(u"hi", 2, 2, -1, -1);
  EXPECT_EQ(info, info_expected);
}

// Test that the diff between the current and previous edited text is calculated
// correctly.
TEST_F(TextInputInfoTest, CommitDiff) {
  // Add a character.
  auto edits = EditedText(Text("a", 1, 1), Text("", 0, 0)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT, u"a", 1));

  // Add more characters.
  edits = EditedText(Text("asdf", 4, 4), Text("a", 1, 1)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"sdf", 3));

  // Delete a character.
  edits = EditedText(Text("asd", 3, 3), Text("asdf", 4, 4)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::DELETE_TEXT, u"", -1));

  // Add characters while the cursor is not at the end.
  edits = EditedText(Text("asqwed", 5, 5), Text("asd", 2, 2)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"qwe", 3));
}

TEST_F(TextInputInfoTest, CommitDiffWithSelection) {
  // There was a selection and the new text is shorter than the selection text.
  auto edits = EditedText(Text("This a text", 6, 6), Text("This is text", 5, 7))
                   .GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT, u"a", 1));

  // There was a selection and the new text is longer than the selection text.
  // This could happen when the user clicks on a keyboard suggestion.
  edits =
      EditedText(Text("This was the text", 12, 12), Text("This is text", 5, 7))
          .GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"was the", 7));
  // There was a selection and the new text is of the same length as the
  // selection.
  edits = EditedText(Text("This ha text", 7, 7), Text("This is text", 5, 7))
              .GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"ha", 2));

  // There was a selection and backspace was pressed.
  edits = EditedText(Text(" text", 0, 0), Text("short text", 0, 5)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::COMMIT_TEXT, u"", 0));
}

TEST_F(TextInputInfoTest, CompositionDiff) {
  // Start composition
  auto edits = EditedText(Text("a", 1, 1, 0, 1), Text("", 0, 0)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::SET_COMPOSING_TEXT, u"a", 1));

  // Add more characters.
  edits = EditedText(Text("asdf", 4, 4, 0, 4), Text("a", 1, 1, 0, 1)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::SET_COMPOSING_TEXT, u"asdf", 3));

  // Delete a few characters.
  edits =
      EditedText(Text("as", 2, 2, 0, 2), Text("asdf", 4, 4, 0, 4)).GetDiff();
  EXPECT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits[0],
            TextEditAction(TextEditActionType::SET_COMPOSING_TEXT, u"as", -2));

  // Finish composition.
  edits =
      EditedText(Text("as ", 3, 3, -1, -1), Text("as", 2, 2, 0, 2)).GetDiff();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"as ", 3));

  // Finish composition, but the text is different. This could happen when the
  // user hits a suggestion that's different from the current composition, but
  // has the same length.
  edits =
      EditedText(Text("lk", 2, 2, -1, -1), Text("as", 2, 2, 0, 2)).GetDiff();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"lk", 2));

  // Finish composition, but the new text is shorter than the previous
  // composition. This could happen when the user hits a suggestion that's
  // shorter than the text they were composing.
  edits =
      EditedText(Text("hi hello", 2, 2, -1, -1), Text("hii hello", 3, 3, 0, 3))
          .GetDiff();
  EXPECT_EQ(edits.size(), 2u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"hi", 2));

  // A new composition without finishing the previous composition. This could
  // happen when we get coalesced events from the keyboard. For example, when
  // the user presses the spacebar and a key right after while there is already
  // an ongoing composition, the keyboard may give us a new composition without
  // finishing the previous composition.
  edits =
      EditedText(Text("hii hello", 3, 3, 2, 3), Text("hi hello", 2, 2, 0, 2))
          .GetDiff();
  EXPECT_EQ(edits.size(), 3u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1],
            TextEditAction(TextEditActionType::COMMIT_TEXT, u"hi", 2));
  EXPECT_EQ(edits[2],
            TextEditAction(TextEditActionType::SET_COMPOSING_TEXT, u"i", 1));
  // Same as above, but the new composition happens by deleting the current
  // composition.
  edits =
      EditedText(Text("hi hello", 2, 2, 0, 2), Text("hii hello", 3, 3, 2, 3))
          .GetDiff();
  EXPECT_EQ(edits.size(), 3u);
  EXPECT_EQ(edits[0], TextEditAction(TextEditActionType::CLEAR_COMPOSING_TEXT));
  EXPECT_EQ(edits[1], TextEditAction(TextEditActionType::DELETE_TEXT, u"", -2));
  EXPECT_EQ(edits[2],
            TextEditAction(TextEditActionType::SET_COMPOSING_TEXT, u"hi", 2));
}

}  // namespace vr
