// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/constants/chromeos_features.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::editor_menu {

namespace {

struct TextAndImageModeTestCase {
  bool magic_boost_revamp_enabled;
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
        // magic_boost_revamp_enabled=false
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kHardBlocked,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kBlocked},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kConsentNeeded,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kPromoCard},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kWrite, LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteOnly},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kRewrite, LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteOnly},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kHardBlocked,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kLobsterWithSelectedText},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kWrite,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteAndLobster},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/false,
                                 EditorMode::kRewrite,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteAndLobster},
        // magic_boost_revamp_enabled=true
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kHardBlocked,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kLobsterWithNoSelectedText},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kHardBlocked,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kLobsterWithSelectedText},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kConsentNeeded,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteOnly},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kConsentNeeded,
                                 LobsterMode::kBlocked,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteOnly},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kConsentNeeded,
                                 LobsterMode::kSelectedText,
                                 EditorMenuCardTextSelectionMode::kHasSelection,
                                 TextAndImageMode::kEditorRewriteAndLobster},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kConsentNeeded,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteAndLobster},
        TextAndImageModeTestCase{/*magic_boost_revamp_enabled=*/true,
                                 EditorMode::kWrite,
                                 LobsterMode::kNoSelectedText,
                                 EditorMenuCardTextSelectionMode::kNoSelection,
                                 TextAndImageMode::kEditorWriteAndLobster},
        TextAndImageModeTestCase{
            /*magic_boost_revamp_enabled=*/true, EditorMode::kRewrite,
            LobsterMode::kSelectedText,
            EditorMenuCardTextSelectionMode::kHasSelection,
            TextAndImageMode::kEditorRewriteAndLobster}, ));

TEST_P(EditorMenuCardContextTextAndImageModeTest, TextAndImageModeIsCorrect) {
  base::test::ScopedFeatureList feature_list;

  feature_list.InitWithFeatureState(chromeos::features::kMagicBoostRevamp,
                                    GetParam().magic_boost_revamp_enabled);

  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(GetParam().editor_mode)
          .set_lobster_mode(GetParam().lobster_mode)
          .set_text_selection_mode(GetParam().text_selection_mode)
          .build();

  EXPECT_EQ(context.text_and_image_mode(), GetParam().expected_mode);
}

struct PresetQueriesTestCase {
  bool magic_boost_revamp_enabled;
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
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kHardBlocked, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kConsentNeeded, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kRewrite,
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
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kWrite, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kHardBlocked,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kConsentNeeded,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kWrite, LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kHardBlocked,
                              LobsterMode::kSelectedText,
                              EditorMenuCardTextSelectionMode::kHasSelection,
                              /*editor_preset_queries=*/
                              {},
                              /*expected_queries=*/
                              {PresetTextQuery(
                                  /*preset_text_id=*/kLobsterPresetId,
                                  GetEditorMenuLobsterChipLabel(),
                                  PresetQueryCategory::kLobster)}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/false,
                              EditorMode::kConsentNeeded,
                              LobsterMode::kSelectedText,
                              EditorMenuCardTextSelectionMode::kHasSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/
                              {}},
        PresetQueriesTestCase{
            /*magic_boost_revamp_enabled=*/false,
            EditorMode::kRewrite,
            LobsterMode::kSelectedText,
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
                             PresetQueryCategory::kUnknown),
             PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                             GetEditorMenuLobsterChipLabel(),
                             PresetQueryCategory::kLobster)}},

        // MagicBoostRevamp enabled
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kHardBlocked, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kConsentNeeded, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kRewrite,
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
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kWrite, LobsterMode::kBlocked,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kHardBlocked,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kConsentNeeded,
                              LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kWrite, LobsterMode::kNoSelectedText,
                              EditorMenuCardTextSelectionMode::kNoSelection,
                              /*editor_preset_queries=*/{},
                              /*expected_queries=*/{}},
        PresetQueriesTestCase{/*magic_boost_revamp_enabled=*/true,
                              EditorMode::kHardBlocked,
                              LobsterMode::kSelectedText,
                              EditorMenuCardTextSelectionMode::kHasSelection,
                              /*editor_preset_queries=*/
                              {},
                              /*expected_queries=*/
                              {PresetTextQuery(
                                  /*preset_text_id=*/kLobsterPresetId,
                                  GetEditorMenuLobsterChipLabel(),
                                  PresetQueryCategory::kLobster)}},
        PresetQueriesTestCase{
            /*magic_boost_revamp_enabled=*/true,
            EditorMode::kConsentNeeded,
            LobsterMode::kSelectedText,
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
                             PresetQueryCategory::kUnknown),
             PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                             GetEditorMenuLobsterChipLabel(),
                             PresetQueryCategory::kLobster)}},
        PresetQueriesTestCase{
            /*magic_boost_revamp_enabled=*/true,
            EditorMode::kRewrite,
            LobsterMode::kSelectedText,
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
                             PresetQueryCategory::kUnknown),
             PresetTextQuery(/*preset_text_id=*/kLobsterPresetId,
                             GetEditorMenuLobsterChipLabel(),
                             PresetQueryCategory::kLobster)}}, )

);

TEST_P(EditorMenuCardContextPresetQueriesTest, PresetQueriesAreCorrect) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      chromeos::features::kMagicBoostRevamp,
      GetParam().magic_boost_revamp_enabled);

  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(GetParam().editor_mode)
          .set_lobster_mode(GetParam().lobster_mode)
          .set_text_selection_mode(
              GetParam().text_selection_mode)  // Add this line
          .set_editor_preset_queries(GetParam().editor_preset_queries)
          .build();

  EXPECT_THAT(context.preset_queries(),
              testing::ElementsAreArray(GetParam().expected_queries));
}

}  // namespace

}  // namespace chromeos::editor_menu
