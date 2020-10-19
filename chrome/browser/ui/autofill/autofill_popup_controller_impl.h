// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <vector>

#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {
struct NativeWebKeyboardEvent;
class WebContents;
}  // namespace content

namespace ui {
class AXPlatformNode;
}

namespace autofill {

class AutofillPopupDelegate;
class AutofillPopupView;

// This class is a controller for an AutofillPopupView. It implements
// AutofillPopupController to allow calls from AutofillPopupView. The
// other, public functions are available to its instantiator.
class AutofillPopupControllerImpl : public AutofillPopupController {
 public:
  // Creates a new |AutofillPopupControllerImpl|, or reuses |previous| if the
  // construction arguments are the same. |previous| may be invalidated by this
  // call. The controller will listen for keyboard input routed to
  // |web_contents| while the popup is showing, unless |web_contents| is NULL.
  static base::WeakPtr<AutofillPopupControllerImpl> GetOrCreate(
      base::WeakPtr<AutofillPopupControllerImpl> previous,
      base::WeakPtr<AutofillPopupDelegate> delegate,
      content::WebContents* web_contents,
      gfx::NativeView container_view,
      const gfx::RectF& element_bounds,
      base::i18n::TextDirection text_direction);

  // Shows the popup, or updates the existing popup with the given values.
  virtual void Show(const std::vector<Suggestion>& suggestions,
                    bool autoselect_first_suggestion,
                    PopupType popup_type);

  // Updates the data list values currently shown with the popup.
  virtual void UpdateDataListValues(const std::vector<base::string16>& values,
                                    const std::vector<base::string16>& labels);

  // Informs the controller that the popup may not be hidden by stale data or
  // interactions with native Chrome UI. This state remains active until the
  // view is destroyed.
  void PinView();

  // Returns (not elided) suggestions currently held by the controller.
  base::span<const Suggestion> GetUnelidedSuggestions() const;

  // Hides the popup and destroys the controller. This also invalidates
  // |delegate_|.
  void Hide(PopupHidingReason reason) override;

  // Invoked when the view was destroyed by by someone other than this class.
  void ViewDestroyed() override;

  bool HandleKeyPressEvent(const content::NativeWebKeyboardEvent& event);

 protected:
  FRIEND_TEST_ALL_PREFIXES(AutofillPopupControllerUnitTest,
                           ProperlyResetController);

  AutofillPopupControllerImpl(base::WeakPtr<AutofillPopupDelegate> delegate,
                              content::WebContents* web_contents,
                              gfx::NativeView container_view,
                              const gfx::RectF& element_bounds,
                              base::i18n::TextDirection text_direction);
  ~AutofillPopupControllerImpl() override;

  void SelectionCleared() override;
  gfx::NativeView container_view() const override;
  content::WebContents* GetWebContents() const override;
  const gfx::RectF& element_bounds() const override;
  void SetElementBounds(const gfx::RectF& bounds);
  bool IsRTL() const override;
  std::vector<Suggestion> GetSuggestions() const override;

  // AutofillPopupController implementation.
  void OnSuggestionsChanged() override;
  void AcceptSuggestion(int index) override;
  int GetLineCount() const override;
  const Suggestion& GetSuggestionAt(int row) const override;
  const base::string16& GetSuggestionValueAt(int row) const override;
  const base::string16& GetSuggestionLabelAt(int row) const override;
  bool GetRemovalConfirmationText(int list_index,
                                  base::string16* title,
                                  base::string16* body) override;
  bool RemoveSuggestion(int list_index) override;
  void SetSelectedLine(base::Optional<int> selected_line) override;
  base::Optional<int> selected_line() const override;
  PopupType GetPopupType() const override;

  // Increase the selected line by 1, properly handling wrapping.
  void SelectNextLine();

  // Decrease the selected line by 1, properly handling wrapping.
  void SelectPreviousLine();

  // The user has removed a suggestion.
  bool RemoveSelectedLine();

  // Returns true if the given id refers to an element that can be accepted.
  bool CanAccept(int id);

  // Returns true if the popup still has non-options entries to show the user.
  bool HasSuggestions();

  // Set the Autofill entry values. Exposed to allow tests to set these values
  // without showing the popup.
  void SetValues(const std::vector<Suggestion>& suggestions);

  AutofillPopupView* view() { return view_; }

  base::WeakPtr<AutofillPopupControllerImpl> GetWeakPtr();

  // Raise an accessibility event to indicate the controls relation of the
  // form control of the popup and popup itself has changed based on the popup's
  // show or hide action.
  void FireControlsChangedEvent(bool is_show);

  // Gets the root AXPlatformNode for our web_contents_, which can be used
  // to find the AXPlatformNode specifically for the autofill text field.
  virtual ui::AXPlatformNode* GetRootAXPlatformNodeForWebContents();

 private:
  // The user has accepted the currently selected line. Returns whether there
  // was a selection to accept.
  bool AcceptSelectedLine();

  // Clear the internal state of the controller. This is needed to ensure that
  // when the popup is reused it doesn't leak values between uses.
  void ClearState();

  // Hides |view_| unless it is null and then deletes |this|.
  void HideViewAndDie();

  friend class AutofillPopupControllerUnitTest;
  friend class AutofillPopupControllerAccessibilityUnitTest;
  void SetViewForTesting(AutofillPopupView* view) { view_ = view; }

  PopupControllerCommon controller_common_;
  content::WebContents* web_contents_;
  AutofillPopupView* view_ = nullptr;  // Weak reference.
  base::WeakPtr<AutofillPopupDelegate> delegate_;

  // If set to true, the popup will never be hidden because of stale data or if
  // the user interacts with native UI.
  bool is_view_pinned_ = false;

  // The current Autofill query values.
  std::vector<Suggestion> suggestions_;

  // The line that is currently selected by the user, null indicates that no
  // line is currently selected.
  base::Optional<int> selected_line_;

  base::WeakPtrFactory<AutofillPopupControllerImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_CONTROLLER_IMPL_H_
