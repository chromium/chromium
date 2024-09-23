// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxPopupView.  Each toolkit
// will implement the popup view differently, so that code is inherently
// platform specific.  However, the OmniboxPopupModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_

#include <stddef.h>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"

class OmniboxController;
class OmniboxEditModel;
namespace ui {
struct AXNodeData;
}

class OmniboxPopupView {
 public:
  explicit OmniboxPopupView(OmniboxController* controller);
  virtual ~OmniboxPopupView();

  virtual OmniboxEditModel* model();
  virtual const OmniboxEditModel* model() const;

  virtual OmniboxController* controller();
  virtual const OmniboxController* controller() const;

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const = 0;

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line) = 0;

  // Invoked when the selection changes. The |line| field in either selection
  // may be OmniboxPopupSelection::kNoMatch. This method is invoked by the
  // model.
  virtual void OnSelectionChanged(OmniboxPopupSelection old_selection,
                                  OmniboxPopupSelection new_selection) {}

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance() = 0;

  // Called to inform result view of button focus.
  virtual void ProvideButtonFocusHint(size_t line) = 0;

  // Notification that the icon used for the given match has been updated.
  virtual void OnMatchIconUpdated(size_t match_index) = 0;

  // This method is called when the view should cancel any active drag (e.g.
  // because the user pressed ESC). The view may or may not need to take any
  // action (e.g. releasing mouse capture).  Note that this can be called when
  // no drag is in progress.
  virtual void OnDragCanceled() = 0;

  // Popup equivalent of GetAccessibleNodeData, used only by a unit test.
  virtual void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) = 0;

  // Called by owning view to get accessibility data.
  virtual void AddPopupAccessibleNodeData(ui::AXNodeData* node_data) = 0;

  // Returns result view button text. This is currently only needed by a single
  // unit test and it would be better to eliminate it than to increase usage.
  virtual std::u16string GetAccessibleButtonTextForResult(size_t line) = 0;

  // Updates the result and header views based on the visibility of their group.
  virtual void SetSuggestionGroupVisibility(size_t match_index,
                                            bool suggestion_group_hidden) {}

  // Adds a callback that will be called when the popup window becomes visible.
  base::CallbackListSubscription AddOpenListener(
      base::RepeatingClosure callback);

 protected:
  // Call when the popup will appear to notify listeners.
  void NotifyOpenListeners();

 private:
  base::RepeatingClosureList on_popup_callbacks_;

  // Owned by OmniboxView which owns this.
  const raw_ptr<OmniboxController> controller_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_POPUP_VIEW_H_
