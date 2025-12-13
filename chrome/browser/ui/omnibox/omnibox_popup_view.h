// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxPopupView.  Each toolkit
// will implement the popup view differently, so that code is inherently
// platform specific.  However, the OmniboxPopupModel needs to do some
// communication with the view.  Since the model is shared between platforms,
// we need to define an interface that all view implementations will share.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_H_

#include <stddef.h>

#include <string_view>

#include "base/memory/raw_ptr.h"

class OmniboxController;
class OmniboxPopupViewWebUI;
class OmniboxResultView;
class OmniboxSuggestionButtonRowView;
namespace ui {
struct AXNodeData;
}

class OmniboxPopupView {
 public:
  explicit OmniboxPopupView(OmniboxController* controller);
  virtual ~OmniboxPopupView();

  // Returns true if the popup is currently open.
  virtual bool IsOpen() const = 0;

  // Invalidates one line of the autocomplete popup.
  virtual void InvalidateLine(size_t line) = 0;

  // Redraws the popup window to match any changes in the result set; this may
  // mean opening or closing the window.
  virtual void UpdatePopupAppearance() = 0;

  // Called to inform result view of button focus.
  virtual void ProvideButtonFocusHint(size_t line) = 0;

  // This method is called when the view should cancel any active drag (e.g.
  // because the user pressed ESC). The view may or may not need to take any
  // action (e.g. releasing mouse capture).  Note that this can be called when
  // no drag is in progress.
  virtual void OnDragCanceled() = 0;

  // Popup equivalent of GetAccessibleNodeData, used only by a unit test.
  virtual void GetPopupAccessibleNodeData(ui::AXNodeData* node_data) const = 0;

  // Returns result view button text. This is currently only needed by a single
  // unit test and it would be better to eliminate it than to increase usage.
  virtual std::u16string_view GetAccessibleButtonTextForResult(
      size_t line) const;

  virtual raw_ptr<OmniboxPopupViewWebUI> GetOmniboxPopupViewWebUI() = 0;

 protected:
  friend class OmniboxResultView;
  friend class OmniboxSuggestionButtonRowView;

  virtual OmniboxController* controller();
  virtual const OmniboxController* controller() const;

 private:
  // Owned by the LocationBarView that owns this. Outlives this.
  const raw_ptr<OmniboxController> controller_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_VIEW_H_
