// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/events/event_handler.h"

class BrowserView;

namespace ui {

class EventTarget;

// Registers itself as PreTargetHandler for `BrowserView`.
// Responsible for closing omnibox popup when user clicks somewhere instead of
// `OmniboxViewViews`.
class OmniboxPopupCloser : public ui::EventHandler {
 public:
  explicit OmniboxPopupCloser(BrowserView* browser_view);
  OmniboxPopupCloser(const OmniboxPopupCloser&) = delete;
  OmniboxPopupCloser& operator=(const OmniboxPopupCloser) = delete;
  ~OmniboxPopupCloser() override;

  // ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  raw_ptr<BrowserView> browser_view_;
  base::ScopedObservation<ui::EventTarget, ui::EventHandler> observer_{this};
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_
