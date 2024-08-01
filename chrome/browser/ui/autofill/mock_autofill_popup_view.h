// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_VIEW_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillPopupView : public AutofillPopupView {
 public:
  MockAutofillPopupView();
  MockAutofillPopupView(MockAutofillPopupView&) = delete;
  MockAutofillPopupView& operator=(MockAutofillPopupView&) = delete;
  ~MockAutofillPopupView() override;

  MOCK_METHOD(bool, Show, (AutoselectFirstSuggestion), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool,
              HandleKeyPressEvent,
              (const input::NativeWebKeyboardEvent&),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsChanged,
              (bool prefer_prev_arrow_side),
              (override));
  MOCK_METHOD(bool, OverlapsWithPictureInPictureWindow, (), (const override));
  MOCK_METHOD(std::optional<int32_t>, GetAxUniqueId, (), (override));
  MOCK_METHOD(void, AxAnnounce, (const std::u16string&), (override));
  MOCK_METHOD(base::WeakPtr<AutofillPopupView>,
              CreateSubPopupView,
              (base::WeakPtr<AutofillSuggestionController>),
              (override));
  MOCK_METHOD(std::optional<AutofillClient::PopupScreenLocation>,
              GetPopupScreenLocation,
              (),
              (const override));
  MOCK_METHOD(bool, HasFocus, (), (const override));

  base::WeakPtr<AutofillPopupView> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AutofillPopupView> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_AUTOFILL_POPUP_VIEW_H_
