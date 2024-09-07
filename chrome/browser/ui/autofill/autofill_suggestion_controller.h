// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/aliases.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillPopupView;
struct PopupControllerCommon;

// This interface provides data to an AutofillPopupView.
class AutofillSuggestionController : public AutofillPopupViewDelegate {
 public:
  // Acts as a factory method to create a new `AutofillSuggestionController`, or
  // reuse `previous` if the construction arguments are the same. `previous` may
  // be invalidated by this call.
  static base::WeakPtr<AutofillSuggestionController> GetOrCreate(
      base::WeakPtr<AutofillSuggestionController> previous,
      base::WeakPtr<AutofillSuggestionDelegate> delegate,
      content::WebContents* web_contents,
      PopupControllerCommon controller_common,
      int32_t form_control_ax_id);

  using UiSessionId = AutofillClient::SuggestionUiSessionId;
  // Generates a new unique session id for suggestion UI.
  static UiSessionId GenerateSuggestionUiSessionId();

  // Recalculates the height and width of the suggestion UI and triggers a
  // redraw when suggestions change.
  virtual void OnSuggestionsChanged() = 0;

  // Accepts the suggestion at `index`. The suggestion is only accepted if the
  // UI has been shown for at least `kIgnoreEarlyClicksOnSuggestionsDuration` to
  // allow ruling out accidental UI interactions (crbug.com/1279268).
  static constexpr base::TimeDelta kIgnoreEarlyClicksOnSuggestionsDuration =
      base::Milliseconds(500);
  virtual void AcceptSuggestion(int index) = 0;

  // Removes the suggestion at the given `index`. `removal_method`specifies the
  // UI entry point for removal, e.g. clicking on a delete button.
  virtual bool RemoveSuggestion(
      int index,
      AutofillMetrics::SingleEntryRemovalMethod removal_method) = 0;

  // Returns the number of lines of data that there are.
  virtual int GetLineCount() const = 0;

  // Returns the full set of autofill suggestions, if applicable.
  virtual const std::vector<Suggestion>& GetSuggestions() const = 0;

  // Returns the suggestion at the given `row` index. The `Suggestion` is the
  // data model including information that is to be shown in the UI.
  virtual const Suggestion& GetSuggestionAt(int row) const = 0;

  // Returns the main filling product corresponding to the controller.
  virtual FillingProduct GetMainFillingProduct() const = 0;

  virtual std::optional<AutofillClient::PopupScreenLocation>
  GetPopupScreenLocation() const = 0;

  // Shows the suggestion UI, or updates the existing suggestion UI with the
  // given values.
  virtual void Show(UiSessionId session_id,
                    std::vector<Suggestion> suggestions,
                    AutofillSuggestionTriggerSource trigger_source,
                    AutoselectFirstSuggestion autoselect_first_suggestion) = 0;

  // Returns the unique session id for the suggestions UI that is showing. If
  // no UI is showing, it returns `std::nullopt`. If there are multiple,
  // connected controllers (e.g. for sub-popups on Desktop), all controllers
  // will have the same session id.
  virtual std::optional<UiSessionId> GetUiSessionId() const = 0;

  // This method cannot be moved into a test api, because it is called by
  // production code in `ChromeAutofillClient`. This happens because, before the
  // popup is shown, tests can ask the client to keep the popup open for
  // testing. Then, once the client shows the popup, the client calls this
  // method.
  virtual void SetKeepPopupOpenForTesting(bool keep_popup_open_for_testing) = 0;

  // Updates the data list values currently shown.
  virtual void UpdateDataListValues(base::span<const SelectOption> options) = 0;

  // Informs the controller that the suggestions may not be hidden by stale data
  // or interactions with native Chrome UI. This state remains active until the
  // view is destroyed.
  virtual void PinView() = 0;

 protected:
  ~AutofillSuggestionController() override = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_SUGGESTION_CONTROLLER_H_
