// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_WINDOW_FEATURE_H_
#define CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_WINDOW_FEATURE_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/tracked_element_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;

namespace tabs {
class ContextHighlightTabFeature;
}

// This class is responsible for managing the AI highlight overlay in a browser
// window. It listens for changes in the active tab and the tracked element
// bounds of that tab, and updates the overlay view accordingly.
class ContextHighlightWindowFeature {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kContextHighlightViewElementId);

  explicit ContextHighlightWindowFeature(BrowserWindowInterface& browser);
  ~ContextHighlightWindowFeature();

  ContextHighlightWindowFeature(const ContextHighlightWindowFeature&) = delete;
  ContextHighlightWindowFeature& operator=(
      const ContextHighlightWindowFeature&) = delete;

  static ContextHighlightWindowFeature* From(BrowserWindowInterface* tab);

  // Called when the tracked element bounds from a
  void CheckAndUpdateTrackedElementBounds();

  DECLARE_USER_DATA(ContextHighlightWindowFeature);

 private:
  // content::TrackedElementObserver:
  void OnTrackedElementBoundsChanged(const cc::TrackedElementBounds& bounds,
                                     float device_scale_factor);

  // Called when the active tab in the browser window changes.
  void OnActiveTabDidChange(BrowserWindowInterface* browser);

  // Returns the ContextHighlightTabFeature for the currently active tab.
  tabs::ContextHighlightTabFeature* GetActiveTabFeature();

  // Sets up the overlay view in the browser window.
  void CreateViewForOverlay();

  raw_ptr<BrowserWindowInterface> browser_;
  views::ViewTracker context_highlight_view_tracker_;
  base::CallbackListSubscription active_tab_subscription_;

  ui::ScopedUnownedUserData<ContextHighlightWindowFeature> scoped_data_;
};

#endif  // CHROME_BROWSER_UI_CONTEXT_HIGHLIGHT_CONTEXT_HIGHLIGHT_WINDOW_FEATURE_H_
