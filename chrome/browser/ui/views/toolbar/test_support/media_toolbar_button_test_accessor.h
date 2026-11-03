// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_MEDIA_TOOLBAR_BUTTON_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_MEDIA_TOOLBAR_BUTTON_TEST_ACCESSOR_H_

#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;
class MediaToolbarButton;

namespace ui {
class TrackedElement;
}  // namespace ui

// Test accessor providing a uniform interface for inspecting and interacting
// with the Global Media Controls toolbar button across both Views
// (`MediaToolbarButtonView`) and WebUI (`WebUIMediaToolbarButton`)
// implementations.
class MediaToolbarButtonTestAccessor {
 public:
  explicit MediaToolbarButtonTestAccessor(BrowserWindowInterface* browser);
  MediaToolbarButtonTestAccessor(const MediaToolbarButtonTestAccessor&) =
      default;
  MediaToolbarButtonTestAccessor& operator=(
      const MediaToolbarButtonTestAccessor&) = default;
  ~MediaToolbarButtonTestAccessor();

  // Returns the underlying `MediaToolbarButton` interface, or nullptr if not
  // present.
  MediaToolbarButton* GetMediaButton() const;

  // Returns true if the media toolbar button is currently visible.
  bool IsVisible() const;

  // Returns the tracked element for the media toolbar button, if one exists.
  ui::TrackedElement* GetElement() const;

  // Simulates a click on the media toolbar button.
  void Click() const;

  // Lays out the browser window if it hasn't already been laid out.
  void LayoutBrowserIfNecessary() const;

 private:
  raw_ptr<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_MEDIA_TOOLBAR_BUTTON_TEST_ACCESSOR_H_
