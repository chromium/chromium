// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace autofill {

class MockAutofillPopupController : public AutofillPopupController {
 public:
  MockAutofillPopupController();
  ~MockAutofillPopupController() override;

  // AutofillPopupViewDelegate:
  MOCK_METHOD(void, Hide, (SuggestionHidingReason), (override));
  MOCK_METHOD(void, ViewDestroyed, (), (override));
  MOCK_METHOD(bool, HasSelection, (), (const override));
  MOCK_METHOD(gfx::Rect, popup_bounds, (), (const override));
  MOCK_METHOD(AutofillSuggestionTriggerSource,
              GetAutofillSuggestionTriggerSource,
              (),
              (const override));
  MOCK_METHOD(bool,
              ShouldIgnoreMouseObservedOutsideItemBoundsCheck,
              (),
              (const override));
  MOCK_METHOD(gfx::NativeView, container_view, (), (const override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (const override));
  const gfx::RectF& element_bounds() const override { return element_bounds_; }
  PopupAnchorType anchor_type() const override { return anchor_type_; }
  void set_element_bounds(const gfx::RectF& bounds) {
    element_bounds_ = bounds;
  }
  MOCK_METHOD(base::i18n::TextDirection,
              GetElementTextDirection,
              (),
              (const override));

  // AutofillSuggestionController:
  MOCK_METHOD(void, OnSuggestionsChanged, (), (override));
  MOCK_METHOD(void, AcceptSuggestion, (int), (override));
  MOCK_METHOD(void,
              PerformButtonActionForSuggestion,
              (int, const SuggestionButtonAction&),
              (override));
  MOCK_METHOD(std::optional<AutofillClient::PopupScreenLocation>,
              GetPopupScreenLocation,
              (),
              (const override));
  const std::vector<Suggestion>& GetSuggestions() const override {
    return suggestions_;
  }
  MOCK_METHOD(const std::vector<SuggestionFilterMatch>&,
              GetSuggestionFilterMatches,
              (),
              (const override));

  int GetLineCount() const override { return suggestions_.size(); }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestions_[row];
  }

  base::WeakPtr<AutofillPopupController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(bool,
              RemoveSuggestion,
              (int, AutofillMetrics::SingleEntryRemovalMethod),
              (override));
  MOCK_METHOD(void, SelectSuggestion, (int), (override));
  MOCK_METHOD(void, UnselectSuggestion, (), (override));
  MOCK_METHOD(FillingProduct, GetMainFillingProduct, (), (const override));
  MOCK_METHOD(base::WeakPtr<AutofillSuggestionController>,
              OpenSubPopup,
              (const gfx::RectF& anchor_bounds,
               std::vector<Suggestion> suggestions,
               AutoselectFirstSuggestion autoselect_first_suggestion),
              (override));
  MOCK_METHOD(void, HideSubPopup, (), (override));
  MOCK_METHOD(void,
              Show,
              (UiSessionId,
               std::vector<Suggestion>,
               AutofillSuggestionTriggerSource,
               AutoselectFirstSuggestion),
              (override));
  MOCK_METHOD(std::optional<AutofillSuggestionController::UiSessionId>,
              GetUiSessionId,
              (),
              (const override));
  MOCK_METHOD(void, SetKeepPopupOpenForTesting, (bool), (override));
  MOCK_METHOD(void,
              UpdateDataListValues,
              (base::span<const SelectOption>),
              (override));
  MOCK_METHOD(void, PinView, (), (override));
  MOCK_METHOD(void, SetFilter, (std::optional<SuggestionFilter>), (override));
  MOCK_METHOD(void, OnPopupPainted, (), (override));
  MOCK_METHOD(bool,
              HandleKeyPressEvent,
              (const input::NativeWebKeyboardEvent& event),
              (override));
  MOCK_METHOD(bool, HasFilteredOutSuggestions, (), (const override));
  MOCK_METHOD(bool,
              IsViewVisibilityAcceptingThresholdEnabled,
              (),
              (const override));

  void set_suggestions(const std::vector<SuggestionType>& ids) {
    suggestions_.clear();

    for (const auto& id : ids) {
      // Accessibility requires all focusable AutofillPopupItemView to have
      // ui::AXNodeData with non-empty names. We specify dummy values and labels
      // to satisfy this.
      suggestions_.emplace_back("dummy_value", "dummy_label",
                                Suggestion::Icon::kNoIcon, id);
    }
  }

  void set_suggestions(std::vector<Suggestion> suggestions) {
    suggestions_ = std::move(suggestions);
  }

  void InvalidateWeakPtrs() { weak_ptr_factory_.InvalidateWeakPtrs(); }

 private:
  std::vector<autofill::Suggestion> suggestions_;
  std::vector<SuggestionFilterMatch> filter_matches_;
  gfx::ScopedDefaultFontDescription default_font_desc_setter_{
      "Arial, Times New Roman, 15px"};
  gfx::RectF element_bounds_ = {100, 100, 250, 50};
  PopupAnchorType anchor_type_ = PopupAnchorType::kField;

  base::WeakPtrFactory<MockAutofillPopupController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_CONTROLLER_H_
