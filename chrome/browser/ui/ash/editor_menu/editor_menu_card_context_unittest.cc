// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"

#include <vector>

#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::editor_menu {

namespace {

struct TextAndImageModeTestCase {
  EditorMode editor_mode;
  LobsterMode lobster_mode;
  EditorMenuCardTextSelectionMode text_selection_mode;
  TextAndImageMode expected_mode;
};

class EditorMenuCardContextTextAndImageModeTest
    : public testing::TestWithParam<TextAndImageModeTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    EditorMenuCardContextTextAndImageModeTest,
    testing::Values(
        TextAndImageModeTestCase{EditorMode::kHardBlocked,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kBlocked},
        TextAndImageModeTestCase{EditorMode::kHardBlocked,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kLobsterWithNoSelectedText},
        TextAndImageModeTestCase{EditorMode::kHardBlocked,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kLobsterWithSelectedText},
        TextAndImageModeTestCase{EditorMode::kConsentNeeded,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteOnly},
        TextAndImageModeTestCase{EditorMode::kConsentNeeded,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteOnly},
        TextAndImageModeTestCase{EditorMode::kConsentNeeded,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteAndLobster},
        TextAndImageModeTestCase{EditorMode::kConsentNeeded,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteAndLobster},
        TextAndImageModeTestCase{EditorMode::kWrite, LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteOnly},
        TextAndImageModeTestCase{EditorMode::kWrite,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteAndLobster},
        TextAndImageModeTestCase{EditorMode::kRewrite, LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteOnly},
        TextAndImageModeTestCase{EditorMode::kRewrite,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteAndLobster}));

TEST_P(EditorMenuCardContextTextAndImageModeTest, TextAndImageModeIsCorrect) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(GetParam().editor_mode)
          .set_lobster_mode(GetParam().lobster_mode)
          .set_text_selection_mode(GetParam().text_selection_mode)
          .build();

  EXPECT_EQ(context.text_and_image_mode(), GetParam().expected_mode);
}

struct PresetQueriesTestCase {
  EditorMode editor_mode;
  LobsterMode lobster_mode;
  EditorMenuCardTextSelectionMode text_selection_mode;
  std::vector<PresetTextQuery> editor_preset_queries;
  std::vector<PresetTextQuery> expected_queries;
};

class EditorMenuCardContextPresetQueriesTest
    : public testing::TestWithParam<PresetQueriesTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    EditorMenuCardContextPresetQueriesTest,
    testing::Values(
        PresetQueriesTestCase{EditorMode::kHardBlocked, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{EditorMode::kHardBlocked,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{EditorMode::kConsentNeeded, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{EditorMode::kConsentNeeded,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{EditorMode::kRewrite,
                              LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kHasSelection,
                              /*editor_preset_queries=*/
                              {PresetTextQuery(/*preset_text_id=*/"1",
                                               u"Query 1",
                                               PresetQueryCategory::kUnknown),
                               PresetTextQuery(/*preset_text_id=*/"2",
                                               u"Query 2",
                                               PresetQueryCategory::kUnknown),
                               PresetTextQuery(/*preset_text_id=*/"3",
                                               u"Query 3",
                                               PresetQueryCategory::kUnknown)},
                              /*expected_queries=*/
                              {PresetTextQuery(/*preset_text_id=*/"1",
                                               u"Query 1",
                                               PresetQueryCategory::kUnknown),
                               PresetTextQuery(/*preset_text_id=*/"2",
                                               u"Query 2",
                                               PresetQueryCategory::kUnknown),
                               PresetTextQuery(/*preset_text_id=*/"3",
                                               u"Query 3",
                                               PresetQueryCategory::kUnknown)}},
        PresetQueriesTestCase{EditorMode::kWrite, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{EditorMode::kWrite, LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}}));

TEST_P(EditorMenuCardContextPresetQueriesTest, PresetQueriesAreCorrect) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(GetParam().editor_mode)
          .set_lobster_mode(GetParam().lobster_mode)
          .set_text_selection_mode(GetParam().text_selection_mode)
          .set_editor_preset_queries(GetParam().editor_preset_queries)
          .build();

  EXPECT_THAT(context.preset_queries(),
              testing::ElementsAreArray(GetParam().expected_queries));
}

// The remaining test cases that involve showing the Lobster chips in the preset
// query chip list. They are written in separate tests since the expected
// strings for Lobster chips can not be passed into the parameterized test as
// normal.
class EditorMenuCardContextWithLobsterChipTest : public testing::Test {};

TEST_F(EditorMenuCardContextWithLobsterChipTest,
       WhenEditorisBlockedAndLobsterIsEnabledWithSelectedText) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kHardBlocked)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .set_text_selection_mode(
              EditorMenuCardTextSelectionMode::kHasSelection)
          .set_editor_preset_queries({})
          .build();

  EXPECT_THAT(
      context.preset_queries(),
      testing::ElementsAreArray({PresetTextQuery(
          /*preset_text_id=*/kLobsterPresetId, GetEditorMenuLobsterChipLabel(),
          PresetQueryCategory::kLobster)}));
}

TEST_F(EditorMenuCardContextWithLobsterChipTest,
       WhenEditorRequiresConsentAndLobsterIsEnabledWithSelectedText) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kConsentNeeded)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .set_text_selection_mode(
              EditorMenuCardTextSelectionMode::kHasSelection)
          .set_editor_preset_queries(
              {PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                               PresetQueryCategory::kUnknown),
               PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                               PresetQueryCategory::kUnknown),
               PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                               PresetQueryCategory::kUnknown)})
          .build();

  EXPECT_THAT(context.preset_queries(),
              testing::ElementsAreArray(
                  {PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                                   GetEditorMenuLobsterChipLabel(),
                                   PresetQueryCategory::kLobster)}));
}

TEST_F(EditorMenuCardContextWithLobsterChipTest,
       WhenEditorisInRewriteModeAndLobsterIsEnabledWithSelectedText) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kRewrite)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .set_text_selection_mode(
              EditorMenuCardTextSelectionMode::kHasSelection)
          .set_editor_preset_queries(
              {PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                               PresetQueryCategory::kUnknown),
               PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                               PresetQueryCategory::kUnknown),
               PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                               PresetQueryCategory::kUnknown)})
          .build();

  EXPECT_THAT(context.preset_queries(),
              testing::ElementsAreArray(
                  {PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                                   PresetQueryCategory::kUnknown),
                   PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                                   GetEditorMenuLobsterChipLabel(),
                                   PresetQueryCategory::kLobster)}));
}

}  // namespace
}  // namespace chromeos::editor_menu
