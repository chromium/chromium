// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include <string>
#include <type_traits>
#include <vector>

#include "base/functional/invoke.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/aliases.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace gfx {
class RectF;
}  // namespace gfx

namespace password_manager {
class ContentPasswordManagerDriver;
}

namespace ui {
class AXPlatformNode;
}

namespace autofill {

class AutofillPopupDelegate;
class AutofillPopupView;
class ContentAutofillDriver;

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl : public AutofillPopupController {
 public:
  AutofillPopupControllerImpl(const AutofillPopupControllerImpl&) = delete;
  AutofillPopupControllerImpl& operator=(const AutofillPopupControllerImpl&) =
      delete;

  // Creates a new `AutofillPopupControllerImpl`, or reuses `previous` if the
  // construction arguments are the same. `previous` may be invalidated by this
  // call. The controller will listen for keyboard input routed to
  // `web_contents` while the popup is showing, unless `web_contents` is NULL.
  static base::WeakPtr<AutofillPopupControllerImpl> GetOrCreate(
      base::WeakPtr<AutofillPopupControllerImpl> previous,
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      gfx::NativeView container_view,
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction);

  // Shows the popup, or updates the existing popup with the given values.
  virtual void Show(std::vector<Suggestion> suggestions,
                    AutoselectFirstSuggestion autoselect_first_suggestion);

  // Updates the data list values currently shown with the popup.
  virtual void UpdateDataListValues(const std::vector<std::u16string>& values,
                                    const std::vector<std::u16string>& labels);

  // Informs the controller that the popup may not be hidden by stale data or
  // interactions with native Chrome UI. This state remains active until the
  // view is destroyed.
  void PinView();

  void KeepPopupOpenForTesting() { keep_popup_open_for_testing_ = true; }

  // Hides the popup and destroys the controller. This also invalidates
  // `delegate_`.
  void Hide(PopupHidingReason reason) override;

  // Invoked when the view was destroyed by by someone other than this class.
  void ViewDestroyed() override;

  // Handles a key press event and returns whether the event should be swallowed
  // (meaning that no other handler, in not particular the default handler, can
  // process it).
  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

  // AutofillPopupController:
  std::vector<Suggestion> GetSuggestions() const override;

  // Disables show thresholds. See the documentation of the member for details.
  void DisableThresholdForTesting(bool disable_threshold) {
    disable_threshold_for_testing_ = disable_threshold;
  }

 protected:
  FRIEND_TEST_ALL_PREFIXES(AutofillPopupControllerUnitTest,
                           ProperlyResetController);

  AutofillPopupControllerImpl(base::WeakPtr<AutofillPopupDelegate> delegate,
                              content::WebContents* web_contents,
                              gfx::NativeView container_view,
                              const gfx::RectF& element_bounds,
                              base::i18n::TextDirection text_direction);
  ~AutofillPopupControllerImpl() override;

  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  void SetElementBounds(const gfx::RectF& bounds);
  base::i18n::TextDirection GetElementTextDirection() const override;

  // AutofillPopupController:
  void OnSuggestionsChanged() override;
  void SelectSuggestion(absl::optional<size_t> index) override;
  void AcceptSuggestion(int index) override;
  void AcceptSuggestionWithoutThreshold(int index) override;
  bool RemoveSuggestion(int list_index) override;
  int GetLineCount() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  std::u16string GetSuggestionMainTextAt(int row) const override;
  std::u16string GetSuggestionMinorTextAt(int row) const override;
  std::vector<std::vector<Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override;
  bool GetRemovalConfirmationText(int list_index,
                                  std::u16string* title,
                                  std::u16string* body) override;
  PopupType GetPopupType() const override;

  // Returns true if the popup still has non-options entries to show the user.
  bool HasSuggestions() const;

  // Set the Autofill entry values. Exposed to allow tests to set these values
  // without showing the popup.
  void SetSuggestions(std::vector<Suggestion> suggestions);

  base::WeakPtr<AutofillPopupControllerImpl> GetWeakPtr();

  // Raise an accessibility event to indicate the controls relation of the
  // form control of the popup and popup itself has changed based on the popup's
  // show or hide action.
  void FireControlsChangedEvent(bool is_show);

  // Gets the root AXPlatformNode for our web_contents_, which can be used
  // to find the AXPlatformNode specifically for the autofill text field.
  virtual ui::AXPlatformNode* GetRootAXPlatformNodeForWebContents();

  // Hides `view_` unless it is null and then deletes `this`.
  virtual void HideViewAndDie();

 private:
  // Wraps a raw AutofillPopupView pointer and checks for nullptr before any
  // dereference. This is useful because AutofillPopupView may be synchronously
  // deleted and set to nullptr by many calls in AutofillPopupControllerImpl,
  // which easily leads to segfaults. See crbug.com/1277218 for the lifecycle
  // management issue in AutofillPopupView.
  class AutofillPopupViewPtr {
   public:
    AutofillPopupViewPtr();
    AutofillPopupViewPtr(const AutofillPopupViewPtr&) = delete;
    AutofillPopupViewPtr& operator=(const AutofillPopupViewPtr&) = delete;
    ~AutofillPopupViewPtr();

    AutofillPopupViewPtr& operator=(base::WeakPtr<AutofillPopupView> ptr) {
      ptr_ = std::move(ptr);
      return *this;
    }

    explicit operator bool() const { return !!ptr_; }

    // If `ptr_ == nullptr`, returns something that converts to false.
    // If `ptr_ != nullptr`, calls `ptr_->func(args...)` and, if that returns a
    // value, returns this value wrapped in an `absl::optional`, otherwise
    // returns true.
    template <typename Func, typename... Args>
    [[nodiscard]] auto Call(Func&& func, Args... args) {
      using ReturnType = decltype(base::invoke(func, *ptr_, args...));
      if constexpr (!std::is_void_v<ReturnType>) {
        return ptr_ ? absl::optional<ReturnType>(
                          base::invoke(func, *ptr_, args...))
                    : absl::optional<ReturnType>();
      } else {
        return ptr_ ? base::invoke(func, *ptr_, args...), true : false;
      }
    }

   private:
    base::WeakPtr<AutofillPopupView> ptr_;
  };

  // Clear the internal state of the controller. This is needed to ensure that
  // when the popup is reused it doesn't leak values between uses.
  void ClearState();

  // Returns true iff the focused frame has a pointer lock, which may be used to
  // trick the user into accepting some suggestion (crbug.com/1239496). In such
  // a case, we should hide the popup.
  bool IsMouseLocked() const;

  // Casts `delegate_->GetDriver()` to ContentAutofillDriver or
  // ContentPasswordManagerDriver, respectively.
  absl::variant<ContentAutofillDriver*,
                password_manager::ContentPasswordManagerDriver*>
  GetDriver();

  friend class AutofillPopupControllerUnitTest;
  friend class AutofillPopupControllerAccessibilityUnitTest;
  void SetViewForTesting(base::WeakPtr<AutofillPopupView> view);

  PopupControllerCommon controller_common_;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  AutofillPopupViewPtr view_;
  base::WeakPtr<AutofillPopupDelegate> delegate_;

  // The time the view was shown the last time. It is used to safeguard against
  // accepting suggestions too quickly after a the popup view was shown (see the
  // `show_threshold` parameter of `AcceptSuggestion`).
  base::TimeTicks time_view_shown_;

  // An override to suppress minimum show thresholds. It should only be set
  // during tests that cannot mock time (e.g. the autofill interactive
  // browsertests).
  bool disable_threshold_for_testing_ = false;

  // If set to true, the popup will never be hidden because of stale data or if
  // the user interacts with native UI.
  bool is_view_pinned_ = false;

  // The current Autofill query values.
  std::vector<Suggestion> suggestions_;

  // If set to true, the popup will stay open regardless of external changes on
  // the machine that would normally cause the popup to be hidden.
  bool keep_popup_open_for_testing_ = false;

  // AutofillPopupControllerImpl deletes itself. To simplify memory management,
  // we delete the object asynchronously.
  base::WeakPtrFactory<AutofillPopupControllerImpl>
      self_deletion_weak_ptr_factory_{this};

  base::WeakPtrFactory<AutofillPopupControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
