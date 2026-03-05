// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/aliases.h"
#include "components/input/native_web_keyboard_event.h"

namespace autofill {

class AutofillSuggestionController;

// The interface for creating and controlling a platform-dependent
// AutofillPopupView.
// TODO(crbug.com/477689220): Check if this interface is still needed, and if
// not, deprecate it. This interface is only implemented by `PopupViewViews` and
// thus is no longer platform-dependent. The struct definitions can then be
// moved to `PopupViewViews`.
class AutofillPopupView {
 public:
  struct SearchBarConfig {
    std::u16string placeholder;
    std::u16string no_results_message;
  };

  // Configuration for displaying a tabbed pane within the Autofill popup.
  struct TabbedPaneConfig {
    enum class TabType {
      kPayNow = 0,
      kPayLater = 1,
    };

    // Represents a single tab to be rendered in the tabbed pane.
    struct Tab {
      TabType type;
      std::u16string title;
    };

    explicit TabbedPaneConfig(std::vector<TabbedPaneConfig::Tab> tabs);
    TabbedPaneConfig(const TabbedPaneConfig&);
    TabbedPaneConfig(TabbedPaneConfig&&);
    TabbedPaneConfig& operator=(const TabbedPaneConfig&);
    TabbedPaneConfig& operator=(TabbedPaneConfig&&);
    ~TabbedPaneConfig();

    // The ordered list of tabs that should be displayed in the tabbed pane.
    std::vector<Tab> tabs;
  };

  // Factory function for creating the view.
  // `search_bar_config` will be used to create a popup with a search bar, if
  // present.
  // `tabbed_pane_config` will be used to create a popup with a tabbed pane,
  // if present.
  static base::WeakPtr<AutofillPopupView> Create(
      base::WeakPtr<AutofillSuggestionController> controller,
      std::optional<const SearchBarConfig> search_bar_config = std::nullopt,
      std::optional<const TabbedPaneConfig> tabbed_pane_config = std::nullopt);

  // Attempts to display the Autofill popup and fills it with data from the
  // controller. Returns whether the popup was shown.
  virtual bool Show(AutoselectFirstSuggestion autoselect_first_suggestion) = 0;

  // Hides the popup from view. This will cause the popup to be deleted.
  virtual void Hide() = 0;

  // Handles a key press event and returns whether the event should be
  // swallowed. This allows views to handle events that depend on its internal
  // state, such as changing the selected Autofill cell.
  virtual bool HandleKeyPressEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Refreshes the position and redraws popup when suggestions change. Returns
  // whether the resulting popup was shown (or had to hide, e.g. due to
  // insufficient size). If `prefer_prev_arrow_side` is `true`, the view takes
  // prev arrow side as the first preferred when recalculating the popup
  // position (potentially changed due to the change of the content).
  virtual void OnSuggestionsChanged(bool prefer_prev_arrow_side) = 0;

  // Returns true if the autofill popup overlaps with the
  // picture-in-picture window.
  virtual bool OverlapsWithPictureInPictureWindow() const = 0;

  // Makes accessibility announcement.
  virtual void AxAnnounce(const std::u16string& text) = 0;

  // Return the autofill popup view's ax unique id.
  virtual std::optional<int32_t> GetAxUniqueId() = 0;

  // Creates a sub-popup (child) view linked to this (parent) view.
  // The child's lifetime depends on its parent, i.e. when the parent dies
  // the child dies also.
  virtual base::WeakPtr<AutofillPopupView> CreateSubPopupView(
      base::WeakPtr<AutofillSuggestionController> sub_controller) = 0;

  // Indicates whether any of the view elements currently has focus.
  virtual bool HasFocus() const = 0;

  // Returns a weak pointer to itself.
  virtual base::WeakPtr<AutofillPopupView> GetWeakPtr() = 0;

 protected:
  virtual ~AutofillPopupView() = default;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
