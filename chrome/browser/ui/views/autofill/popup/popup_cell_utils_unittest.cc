// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_cell_utils.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/throbber.h"

namespace autofill {
namespace {

constexpr char16_t kVirtualCardBadgeLabel[] = u"Virtual card";
constexpr char16_t kIbanBadgeLabel[] = u"IBAN";

struct VoiceOverTestParam {
  Suggestion suggestion;
  std::u16string expected_voice_over;
  std::string test_name;
};

class GetVoiceOverStringFromSuggestionTest
    : public testing::Test,
      public testing::WithParamInterface<VoiceOverTestParam> {
 public:
  GetVoiceOverStringFromSuggestionTest() = default;
  ~GetVoiceOverStringFromSuggestionTest() override = default;
};

TEST_P(GetVoiceOverStringFromSuggestionTest, Test) {
  const VoiceOverTestParam& param = GetParam();
  EXPECT_EQ(
      popup_cell_utils::GetVoiceOverStringFromSuggestion(param.suggestion),
      param.expected_voice_over)
      << "Test case: " << param.test_name;
}

const char* GetExpandableMenuIconNameFromSuggestionType(SuggestionType type) {
  return popup_cell_utils::GetExpandableMenuIcon(type).name;
}

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_ComposeSuggestions_ReturnThreeDotsMenuIcon) {
  EXPECT_EQ(GetExpandableMenuIconNameFromSuggestionType(
                SuggestionType::kComposeProactiveNudge),
            kBrowserToolsChromeRefreshIcon.name);
  // No other Compose type should allow an expandable menu.
  EXPECT_FALSE(IsExpandableSuggestionType(SuggestionType::kComposeResumeNudge));
  EXPECT_FALSE(IsExpandableSuggestionType(
      SuggestionType::kComposeSavedStateNotification));
}

TEST(PopupCellUtilsTest,
     GetExpandableMenuIcon_NonComposeSuggestions_ReturnSubMenuArrowIcon) {
  EXPECT_EQ(GetExpandableMenuIconNameFromSuggestionType(
                SuggestionType::kAddressEntry),
            vector_icons::kSubmenuArrowChromeRefreshIcon.name);
}

const VoiceOverTestParam kVoiceOverTestCases[] = {
    // This is a VCN suggestion without either product description nor
    // card nickname.
    {.suggestion =
         [] {
           Suggestion suggestion(u"Amex ••1234",
                                 SuggestionType::kVirtualCreditCardEntry);
           suggestion.minor_texts = {Suggestion::Text(u"Expires 01/25")};
           return suggestion;
         }(),
     .expected_voice_over =
         u"Amex ••1234 " + std::u16string(kVirtualCardBadgeLabel),
     .test_name = "VCNWithMinorText"},
    // This is a VCN suggestion with a product description.
    {.suggestion =
         [] {
           Suggestion suggestion(u"American Express Gold card",
                                 SuggestionType::kVirtualCreditCardEntry);
           suggestion.labels = {{Suggestion::Text(u"Amex ••1234")}};
           return suggestion;
         }(),
     .expected_voice_over = u"American Express Gold card Amex ••1234 " +
                            std::u16string(kVirtualCardBadgeLabel),
     .test_name = "VCNWithLabels"},
    {.suggestion =
         [] {
           Suggestion suggestion(u"DE ••6199", SuggestionType::kIbanEntry);
           return suggestion;
         }(),
     .expected_voice_over = u"DE ••6199 " + std::u16string(kIbanBadgeLabel),
     .test_name = "IBANWithNoLabels"},
    {.suggestion =
         [] {
           Suggestion suggestion(u"My IBAN", SuggestionType::kIbanEntry);
           suggestion.labels = {{Suggestion::Text(u"DE ••6199")}};
           return suggestion;
         }(),
     .expected_voice_over =
         u"My IBAN DE ••6199 " + std::u16string(kIbanBadgeLabel),
     .test_name = "IBANWithLabels"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    GetVoiceOverStringFromSuggestionTest,
    testing::ValuesIn(kVoiceOverTestCases),
    [](const testing::TestParamInfo<VoiceOverTestParam>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace autofill
