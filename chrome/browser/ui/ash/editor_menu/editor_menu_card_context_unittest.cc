// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/editor_menu/editor_menu_card_context.h"

#include "chrome/browser/ui/ash/editor_menu/editor_menu_strings.h"
#include "chrome/browser/ui/ash/editor_menu/utils/text_and_image_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos::editor_menu {

namespace {

using ::testing::ElementsAre;

class EditorMenuCardContextTest : public testing::Test {
 public:
  EditorMenuCardContextTest() = default;

  EditorMenuCardContextTest(const EditorMenuCardContextTest&) = delete;
  EditorMenuCardContextTest& operator=(const EditorMenuCardContextTest&) =
      delete;

  ~EditorMenuCardContextTest() override = default;
};

TEST_F(EditorMenuCardContextTest, BlockedMode) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kHardBlocked)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_EQ(context.text_and_image_mode(), TextAndImageMode::kBlocked);
}

TEST_F(EditorMenuCardContextTest, PromoCardMode) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kPromoCard)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_EQ(context.text_and_image_mode(), TextAndImageMode::kPromoCard);
}

TEST_F(EditorMenuCardContextTest, EditorWriteOnly) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kWrite)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_EQ(context.text_and_image_mode(), TextAndImageMode::kEditorWriteOnly);
}

TEST_F(EditorMenuCardContextTest, EditorRewriteOnly) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kRewrite)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_EQ(context.text_and_image_mode(),
            TextAndImageMode::kEditorRewriteOnly);
}

TEST_F(EditorMenuCardContextTest, LobsterWithSelectedText) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kHardBlocked)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .build();

  EXPECT_EQ(context.text_and_image_mode(),
            TextAndImageMode::kLobsterWithSelectedText);
}

TEST_F(EditorMenuCardContextTest, EditorWriteAndLobster) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kWrite)
          .set_lobster_mode(LobsterMode::kNoSelectedText)
          .build();

  EXPECT_EQ(context.text_and_image_mode(),
            TextAndImageMode::kEditorWriteAndLobster);
}

TEST_F(EditorMenuCardContextTest, EditorRewriteAndLobster) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kRewrite)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .build();

  EXPECT_EQ(context.text_and_image_mode(),
            TextAndImageMode::kEditorRewriteAndLobster);
}

TEST_F(EditorMenuCardContextTest,
       ReturnsEditorPresetQueriesWhenLobsterIsBlocked) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kRewrite)
          .set_lobster_mode(LobsterMode::kBlocked)
          .set_editor_preset_queries({
              PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                              PresetQueryCategory::kUnknown),
              PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                              PresetQueryCategory::kUnknown),
              PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                              PresetQueryCategory::kUnknown),
          })
          .build();

  EXPECT_THAT(context.preset_queries(),
              ElementsAre(PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                                          PresetQueryCategory::kUnknown),
                          PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                                          PresetQueryCategory::kUnknown),
                          PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                                          PresetQueryCategory::kUnknown)));
}

TEST_F(EditorMenuCardContextTest, ReturnsNoChipsWhenBothFeaturesAreBlocked) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kHardBlocked)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_TRUE(context.preset_queries().empty());
}

TEST_F(EditorMenuCardContextTest, ReturnsNoChipsInPromoCardMode) {
  EditorMenuCardContext context = EditorMenuCardContext()
                                      .set_editor_mode(EditorMode::kPromoCard)
                                      .set_lobster_mode(LobsterMode::kBlocked)
                                      .build();

  EXPECT_TRUE(context.preset_queries().empty());
}

TEST_F(EditorMenuCardContextTest,
       ReturnsNoChipsWhenBothFeaturesAreEnabledInWriteMode) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kWrite)
          .set_lobster_mode(LobsterMode::kNoSelectedText)
          .build();

  EXPECT_TRUE(context.preset_queries().empty());
}

TEST_F(EditorMenuCardContextTest,
       ReturnsEditorAndLobsterChipWhenBothFeaturesAreEnabledInRewriteMode) {
  EditorMenuCardContext context =
      EditorMenuCardContext()
          .set_editor_mode(EditorMode::kRewrite)
          .set_lobster_mode(LobsterMode::kSelectedText)
          .set_editor_preset_queries({
              PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                              PresetQueryCategory::kUnknown),
              PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                              PresetQueryCategory::kUnknown),
              PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                              PresetQueryCategory::kUnknown),
          })
          .build();

  EXPECT_THAT(context.preset_queries(),
              ElementsAre(PresetTextQuery(/*preset_text_id=*/"1", u"Query 1",
                                          PresetQueryCategory::kUnknown),
                          PresetTextQuery(/*preset_text_id=*/"2", u"Query 2",
                                          PresetQueryCategory::kUnknown),
                          PresetTextQuery(/*preset_text_id=*/"3", u"Query 3",
                                          PresetQueryCategory::kUnknown),
                          PresetTextQuery(
                              /*preset_text_id=*/kLobsterPresetId,
                              GetEditorMenuLobsterChipLabel(),
                              PresetQueryCategory::kLobster)));
}

}  // namespace

}  // namespace chromeos::editor_menu
