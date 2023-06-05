// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_adapter.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect_f.h"

using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace autofill {
namespace {

class MockAccessoryView
    : public AutofillKeyboardAccessoryAdapter::AccessoryView {
 public:
  MockAccessoryView() {}

  MockAccessoryView(const MockAccessoryView&) = delete;
  MockAccessoryView& operator=(const MockAccessoryView&) = delete;

  MOCK_METHOD(bool, Initialize, ());
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, Show, ());
  MOCK_METHOD(void, AxAnnounce, (const std::u16string&));
  MOCK_METHOD(void,
              ConfirmDeletion,
              (const std::u16string&,
               const std::u16string&,
               base::OnceClosure));
};

Suggestion createPasswordEntry(std::string password,
                               std::string username,
                               std::string psl_origin) {
  Suggestion s(/*main_text=*/username, /*label=*/psl_origin, /*icon=*/"",
               PopupItemId::kAutocompleteEntry);
  s.additional_label = ASCIIToUTF16(password);
  return s;
}

std::vector<Suggestion> createSuggestions() {
  std::vector<Suggestion> suggestions = {
      createPasswordEntry("****************", "Alf", ""),
      createPasswordEntry("****************", "Berta", "psl.origin.eg"),
      createPasswordEntry("***", "Carl", "")};
  return suggestions;
}

std::vector<Suggestion> createSuggestions(int clearItemOffset) {
  std::vector<Suggestion> suggestions = createSuggestions();
  suggestions.emplace(suggestions.begin() + clearItemOffset, "Clear", "", "",
                      PopupItemId::kClearForm);
  return suggestions;
}

// Convert the Suggestion::labels into one string of format "[[a, b],[c]]".
std::string SuggestionLabelsToString(
    const std::vector<std::vector<Suggestion::Text>>& labels) {
  std::string result;
  for (const std::vector<Suggestion::Text>& texts : labels) {
    if (!result.empty())
      result.append(",");

    std::string row;
    for (const Suggestion::Text& text : texts) {
      if (!row.empty())
        row.append(",");
      row.append(base::UTF16ToUTF8(text.value));
    }
    result.append("[" + row + "]");
  }
  return "[" + result + "]";
}

// Matcher returning true if suggestions have equal members.
MATCHER_P(equalsSuggestion, other, "") {
  if (arg.popup_item_id != other.popup_item_id) {
    *result_listener << "has popup_item_id "
                     << base::to_underlying(arg.popup_item_id);
    return false;
  }
  if (arg.main_text != other.main_text) {
    *result_listener << "has main_text " << arg.main_text.value;
    return false;
  }
  if (arg.labels != other.labels) {
    *result_listener << "has labels " << SuggestionLabelsToString(arg.labels);
    return false;
  }
  if (arg.icon != other.icon) {
    *result_listener << "has icon " << arg.icon;
    return false;
  }
  return true;
}

}  // namespace

class AutofillKeyboardAccessoryAdapterTest : public testing::Test {
 public:
  AutofillKeyboardAccessoryAdapterTest()
      : popup_controller_(
            std::make_unique<StrictMock<MockAutofillPopupController>>()) {
    auto view = std::make_unique<StrictMock<MockAccessoryView>>();
    accessory_view_ = view.get();

    autofill_accessory_adapter_ =
        std::make_unique<AutofillKeyboardAccessoryAdapter>(
            popup_controller_->GetWeakPtr());
    autofill_accessory_adapter_->SetAccessoryView(std::move(view));
  }

  void NotifyAboutSuggestions() {
    EXPECT_CALL(*view(), Show());

    adapter_as_view()->OnSuggestionsChanged();

    testing::Mock::VerifyAndClearExpectations(view());
  }

  const Suggestion& suggestion(int i) {
    return controller()->GetSuggestionAt(i);
  }

  AutofillPopupController* adapter_as_controller() {
    return autofill_accessory_adapter_.get();
  }

  AutofillPopupView* adapter_as_view() {
    return autofill_accessory_adapter_.get();
  }

  MockAutofillPopupController* controller() { return popup_controller_.get(); }

  MockAccessoryView* view() { return accessory_view_; }

 private:
  raw_ptr<StrictMock<MockAccessoryView>> accessory_view_;
  std::unique_ptr<StrictMock<MockAutofillPopupController>> popup_controller_;
  std::unique_ptr<AutofillKeyboardAccessoryAdapter> autofill_accessory_adapter_;
};

TEST_F(AutofillKeyboardAccessoryAdapterTest, ShowingInitializesAndUpdatesView) {
  EXPECT_CALL(*view(), Show());
  adapter_as_view()->Show(AutoselectFirstSuggestion(false));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, HidingAdapterHidesView) {
  EXPECT_CALL(*view(), Hide());
  adapter_as_view()->Hide();
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, ReorderUpdatedSuggestions) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/2));
  EXPECT_CALL(*view(), Show());

  adapter_as_view()->OnSuggestionsChanged();

  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(0),
              equalsSuggestion(suggestion(2)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(1),
              equalsSuggestion(suggestion(0)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(2),
              equalsSuggestion(suggestion(1)));
  EXPECT_THAT(adapter_as_controller()->GetSuggestionAt(3),
              equalsSuggestion(suggestion(3)));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, UseAdditionalLabelForElidedLabel) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/1));
  NotifyAboutSuggestions();

  // The 1st item is usually not visible (something like clear form) and has an
  // empty label. But it needs to be handled since UI might ask for it anyway.
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(0).size(), 1U);
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(0)[0].size(), 1U);
  EXPECT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(0)[0][0].value,
            std::u16string());

  // If there is a label, use it but cap at 8 bullets.
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(1).size(), 1U);
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(1)[0].size(), 1U);
  EXPECT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(1)[0][0].value,
            u"********");

  // If the label is empty, use the additional label:
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(2).size(), 1U);
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(2)[0].size(), 1U);
  EXPECT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(2)[0][0].value,
            u"psl.origin.eg ********");

  // If the password has less than 8 bullets, show the exact amount.
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(3).size(), 1U);
  ASSERT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(3)[0].size(), 1U);
  EXPECT_EQ(adapter_as_controller()->GetSuggestionLabelsAt(3)[0][0].value,
            u"***");
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, ProvideReorderedSuggestions) {
  controller()->set_suggestions(createSuggestions(/*clearItemOffset=*/2));
  NotifyAboutSuggestions();

  EXPECT_THAT(adapter_as_controller()->GetSuggestions(),
              testing::ElementsAre(equalsSuggestion(suggestion(2)),
                                   equalsSuggestion(suggestion(0)),
                                   equalsSuggestion(suggestion(1)),
                                   equalsSuggestion(suggestion(3))));
}

TEST_F(AutofillKeyboardAccessoryAdapterTest, RemoveAfterConfirmation) {
  controller()->set_suggestions(createSuggestions());
  NotifyAboutSuggestions();

  base::OnceClosure confirm;
  EXPECT_CALL(*controller(), GetRemovalConfirmationText(0, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*view(), ConfirmDeletion(_, _, _))
      .WillOnce(WithArg<2>(Invoke([&](base::OnceClosure closure) -> void {
        confirm = std::move(closure);
      })));
  EXPECT_TRUE(adapter_as_controller()->RemoveSuggestion(0));

  EXPECT_CALL(*controller(), RemoveSuggestion(0)).WillOnce(Return(true));
  std::move(confirm).Run();
}

}  // namespace autofill
