// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/next_idle_barrier.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/ui/popup_interaction.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/common/aliases.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace ui {
class AXPlatformNode;
}

namespace autofill {

class AutofillSuggestionDelegate;
class AutofillPopupView;

// Sub-popups and their parent popups are connected by providing children
// with links to their parents. This interface defines the API exposed by
// these links.
class ExpandablePopupParentControllerImpl {
 private:
  friend class AutofillPopupControllerImpl;

  // Creates a view for a sub-popup. On rare occasions opening the sub-popup
  // may fail (e.g. when there is no room to open the sub-popup or the popup
  // is in the middle of destroying and  has no widget already),
  // `nullptr` is returned in these cases.
  virtual base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> sub_controller) = 0;

  // Returns the number of popups above this one. For example, if `this` is the
  // second popup, `GetPopupLevel()` returns 1, if `this` is the root popup,
  // it returns 0.
  virtual int GetPopupLevel() const = 0;
};

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl
    : public AutofillPopupController,
      public ExpandablePopupParentControllerImpl {
 public:
  AutofillPopupControllerImpl(const AutofillPopupControllerImpl&) = delete;
  AutofillPopupControllerImpl& operator=(const AutofillPopupControllerImpl&) =
      delete;

  // AutofillSuggestionController:
  void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index) override;
  bool RemoveSuggestion(
      int list_index,
      AutofillMetrics::SingleEntryRemovalMethod removal_method) override;
  int GetLineCount() const override;
  const std::vector<Suggestion>& GetSuggestions() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  FillingProduct GetMainFillingProduct() const override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  void Hide(SuggestionHidingReason reason) override;
  void ViewDestroyed() override;
  void Show(UiSessionId ui_session_id,
            std::vector<Suggestion> suggestions,
            AutofillSuggestionTriggerSource trigger_source,
            AutoselectFirstSuggestion autoselect_first_suggestion) override;
  std::optional<UiSessionId> GetUiSessionId() const override;
  void SetKeepPopupOpenForTesting(bool keep_popup_open_for_testing) override;
  void UpdateDataListValues(base::span<const SelectOption> options) override;
  void PinView() override;
  bool IsViewVisibilityAcceptingThresholdEnabled() const override;

  // AutofillPopupController:
  void SelectSuggestion(int index) override;
  void UnselectSuggestion() override;
  base::WeakPtr<AutofillSuggestionController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions,
      AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void HideSubPopup() override;
  bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const override;
  void PerformButtonActionForSuggestion(
      int index,
      const SuggestionButtonAction& button_action) override;
  const std::vector<SuggestionFilterMatch>& GetSuggestionFilterMatches()
      const override;
  void SetFilter(std::optional<SuggestionFilter> filter) override;
  bool HasFilteredOutSuggestions() const override;
  bool HandleKeyPressEvent(const input::NativeWebKeyboardEvent& event) override;
  void OnPopupPainted() override;
  base::WeakPtr<AutofillPopupController> GetWeakPtr() override;

 protected:
  AutofillPopupControllerImpl(
      base::WeakPtr<AutofillSuggestionDelegate> delegate,
      content::WebContents* web_contents,
      PopupControllerCommon controller_common,
      int32_t form_control_ax_id,
      std::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>> parent =
          std::nullopt);
  ~AutofillPopupControllerImpl() override;

  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  PopupAnchorType anchor_type() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // Returns true if the popup still has non-options entries to show the user.
  bool HasSuggestions() const;

  // Set the Autofill entry values. Exposed to allow tests to set these values
  // without showing the popup.
  void SetSuggestions(std::vector<Suggestion> suggestions);

  // Raise an accessibility event to indicate the controls relation of the
  // form control of the popup and popup itself has changed based on the popup's
  // show or hide action.
  void FireControlsChangedEvent(bool is_show);

  // Gets the root AXPlatformNode for our WebContents, which can be used
  // to find the AXPlatformNode specifically for the autofill text field.
  virtual ui::AXPlatformNode* GetRootAXPlatformNodeForWebContents();

  // Hides `view_` unless it is null and then deletes `this`.
  virtual void HideViewAndDie();

 private:
  friend class AutofillPopupControllerImplTestApi;
  friend class AutofillSuggestionController;

  // Clear the internal state of the controller. This is needed to ensure that
  // when the popup is reused it doesn't leak values between uses.
  void ClearState();

  // ExpandablePopupParentControllerImpl:
  base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillPopupController> controller) override;
  int GetPopupLevel() const override;

  // Returns `true` if this popup has no parent, and `false` for sub-popups.
  bool IsRootPopup() const;

  // Notifies the view that the suggestions provided by the controller changed.
  // If `prefer_prev_arrow_side` is `true`, the view takes prev arrow side as
  // the first preferred when recalculating the popup position.
  void OnSuggestionsChanged(bool prefer_prev_arrow_side);

  void UpdateFilteredSuggestions();

  UiSessionId ui_session_id_;
  base::WeakPtr<content::WebContents> web_contents_;
  PopupControllerCommon controller_common_;
  base::WeakPtr<AutofillPopupView> view_;
  base::WeakPtr<AutofillSuggestionDelegate> delegate_;

  // A helper class for capturing key press events associated with a
  // `content::RenderFrameHost`.
  class KeyPressObserver {
   public:
    explicit KeyPressObserver(AutofillPopupControllerImpl* observer);
    ~KeyPressObserver();

    void Observe(content::RenderFrameHost* rfh);
    void Reset();

   private:
    const raw_ref<AutofillPopupControllerImpl> observer_;
    content::GlobalRenderFrameHostId rfh_;
    content::RenderWidgetHost::KeyPressEventCallback handler_;
  } key_press_observer_{this};

  // Whether a sufficient amount of time has passed since showing or updating
  // suggestions. It is used to safeguard against accepting suggestions too
  // quickly after a the popup view was shown (see the `show_threshold`
  // parameter of `AcceptSuggestion`).
  std::optional<NextIdleBarrier> barrier_for_accepting_;

  // The time of the latest successful (the view is created and shown) `Show()`
  // call.
  std::optional<base::TimeTicks> shown_time_;

  // An override to suppress minimum show thresholds. It should only be set
  // during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  bool disable_threshold_for_testing_ = false;

  // If set to true, the popup will never be hidden because of stale data or if
  // the user interacts with native UI.
  bool is_view_pinned_ = false;

  // If `filter_` set, it contains suggestions from `non_filtered_suggestions_`
  // that matches the filter.  Otherwise, the list is empty
  std::vector<Suggestion> filtered_suggestions_;

  // Original list of suggestions provided via `SetSuggestions()`.
  std::vector<Suggestion> non_filtered_suggestions_;

  // The trigger source of the `suggestions_`.
  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  // The AX ID of the field on which Autofill was triggered.
  int32_t form_control_ax_id_ = 0;

  // If set to true, the popup will stay open regardless of external changes on
  // the machine that would normally cause the popup to be hidden.
  bool keep_popup_open_for_testing_ = false;

  // Timer to close a fading popup.
  base::OneShotTimer fading_popup_timer_;

  // Whether the popup should ignore mouse observed outside check.
  bool should_ignore_mouse_observed_outside_item_bounds_check_ = false;

  // Parent's popup controller. The root popup doesn't have a parent, but in
  // sub-popups it must be present.
  const std::optional<base::WeakPtr<ExpandablePopupParentControllerImpl>>
      parent_controller_;

  // The open sub-popup controller if any, `nullptr` otherwise.
  base::WeakPtr<AutofillPopupControllerImpl> sub_popup_controller_;

  // This is a helper which detects events that should hide the popup.
  std::optional<AutofillPopupHideHelper> popup_hide_helper_;

  // The filter narrows down the list of suggestions from
  // `non_filtered_suggestions_`. This filtered list is cached in
  // `filtered_suggestions_` and becomes the current data used by clients
  // through the provided API.
  std::optional<SuggestionFilter> filter_;

  // Cached matches, one per suggestion in `filtered_suggestions_` if
  // the `filter_` is set, otherwise it is an empty vector.
  std::vector<SuggestionFilterMatch> suggestion_filter_matches_;

  // The `FillingProduct` that matches the suggestions shown in the popup.
  // The first `IsStandaloneSuggestionType()` is used to define what the
  // `FillingProduct` is.
  FillingProduct suggestions_filling_product_ = FillingProduct::kNone;

  // Whether any suggestion has been selected.
  bool any_suggestion_selected_ = false;

  // AutofillPopupControllerImpl deletes itself. To simplify memory management,
  // we delete the object asynchronously.
  base::WeakPtrFactory<AutofillPopupControllerImpl>
      self_deletion_weak_ptr_factory_{this};

  base::WeakPtrFactory<AutofillPopupControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
