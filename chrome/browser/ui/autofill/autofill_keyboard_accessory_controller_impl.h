// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_hide_helper.h"
#include "chrome/browser/ui/autofill/next_idle_time_ticks.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace content {
class WebContents;
}  // namespace content

class Profile;

namespace autofill {

class AutofillPopupDelegate;
class AutofillPopupView;
struct Suggestion;

class AutofillKeyboardAccessoryControllerImpl
    : public AutofillKeyboardAccessoryController {
 public:
  using ShowPasswordMigrationWarningCallback = base::RepeatingCallback<void(
      gfx::NativeWindow,
      Profile*,
      password_manager::metrics_util::PasswordMigrationWarningTriggers)>;

  AutofillKeyboardAccessoryControllerImpl(
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      PopupControllerCommon controller_common,
      ShowPasswordMigrationWarningCallback show_pwd_migration_warning_callback);

  AutofillKeyboardAccessoryControllerImpl(
      const AutofillKeyboardAccessoryControllerImpl&) = delete;
  AutofillKeyboardAccessoryControllerImpl& operator=(
      const AutofillKeyboardAccessoryControllerImpl&) = delete;

  ~AutofillKeyboardAccessoryControllerImpl() override;

  // AutofillPopupViewDelegate:
  void Hide(PopupHidingReason reason) override;
  void ViewDestroyed() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  base::i18n::TextDirection GetElementTextDirection() const override;

  // AutofillPopupController:
  void OnSuggestionsChanged() override;
  void SelectSuggestion(int index) override;
  void UnselectSuggestion() override;
  void AcceptSuggestion(int index) override;
  void PerformButtonActionForSuggestion(int index) override;
  bool RemoveSuggestion(
      int index,
      AutofillMetrics::SingleEntryRemovalMethod removal_method) override;
  int GetLineCount() const override;
  std::vector<Suggestion> GetSuggestions() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  std::u16string GetSuggestionMainTextAt(int row) const override;
  std::u16string GetSuggestionMinorTextAt(int row) const override;
  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override;
  bool GetRemovalConfirmationText(int index,
                                  std::u16string* title,
                                  std::u16string* body) override;
  FillingProduct GetMainFillingProduct() const override;
  bool ShouldIgnoreMouseObservedOutsideItemBoundsCheck() const override;
  base::WeakPtr<AutofillPopupController> OpenSubPopup(
      const gfx::RectF& anchor_bounds,
      std::vector<Suggestion> suggestions,
      AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void HideSubPopup() override;
  std::optional<AutofillClient::PopupScreenLocation> GetPopupScreenLocation()
      const override;
  void Show(std::vector<Suggestion> suggestions,
            AutofillSuggestionTriggerSource trigger_source,
            AutoselectFirstSuggestion autoselect_first_suggestion) override;
  void DisableThresholdForTesting(bool disable_threshold) override;
  void KeepPopupOpenForTesting() override;
  void SetViewForTesting(base::WeakPtr<AutofillPopupView> view) override;
  void UpdateDataListValues(base::span<const SelectOption> options) override;
  void PinView() override;

  base::WeakPtr<AutofillKeyboardAccessoryControllerImpl> GetWeakPtr();

 private:
  friend class AutofillPopupController;

  // Returns true if the popup has entries that are not "Manage ...".
  bool HasSuggestions() const;

  // Hides the view and asynchronously deletes itself.
  void HideViewAndDie();

  base::WeakPtr<AutofillPopupDelegate> delegate_;
  base::WeakPtr<content::WebContents> web_contents_;
  PopupControllerCommon controller_common_;
  base::WeakPtr<AutofillPopupView> view_;

  // A helper that detects events that should hide the popup.
  std::optional<AutofillPopupHideHelper> popup_hide_helper_;

  // The suggestions to be shown in the Keyboard Accessory.
  std::vector<Suggestion> suggestions_;

  // The trigger source of the `suggestions_`.
  AutofillSuggestionTriggerSource trigger_source_ =
      AutofillSuggestionTriggerSource::kUnspecified;

  // The time the view was shown the last time. It is used to safeguard against
  // accepting suggestions too quickly after a the popup view was shown (see the
  // `show_threshold` parameter of `AcceptSuggestion`).
  NextIdleTimeTicks time_view_shown_;

  // An override to suppress minimum show thresholds. It should only be set
  // during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  bool disable_threshold_for_testing_ = false;

  // If set to true, the popup will never be hidden because of stale data or if
  // the user interacts with native UI.
  bool is_view_pinned_ = false;

  // If set to true, the popup will stay open regardless of external changes on
  // the machine that would normally cause the popup to be hidden.
  bool keep_popup_open_for_testing_ = false;

  // Callback invoked to try to show the password migration warning on Android.
  // Used to facilitate testing.
  // TODO(crbug.com/1454469): Remove when the warning isn't needed anymore.
  ShowPasswordMigrationWarningCallback show_pwd_migration_warning_callback_;

  base::WeakPtrFactory<AutofillKeyboardAccessoryControllerImpl>
      self_deletion_weak_ptr_factory_{this};

  base::WeakPtrFactory<AutofillKeyboardAccessoryControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_CONTROLLER_IMPL_H_
