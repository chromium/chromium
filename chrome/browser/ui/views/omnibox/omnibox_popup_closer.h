// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/events/event_handler.h"

class BrowserView;

namespace omnibox {

class EventTarget;

enum class PopupCloseReason {
  kBlur,
  kBrowserWidgetMoved,
  kEscapeKeyPressed,
  kLocationIconDragged,
  kMouseClickOutside,
  kRevertAll,
  kTextDrag,
  kOther
};

// Closes the omnibox popup when appropriate events or user interactions occur.
class OmniboxPopupCloser : public ui::EventHandler {
 public:
  explicit OmniboxPopupCloser(BrowserView* browser_view);
  OmniboxPopupCloser(const OmniboxPopupCloser&) = delete;
  OmniboxPopupCloser& operator=(const OmniboxPopupCloser) = delete;
  ~OmniboxPopupCloser() override;

  // Closes the omnibox popup for the given reason.
  void CloseWithReason(PopupCloseReason reason);

 private:
  // ui::EventHandler
  void OnMouseEvent(ui::MouseEvent* event) override;

  raw_ptr<BrowserView> browser_view_;
  base::ScopedObservation<ui::EventTarget, ui::EventHandler>
      browser_view_observation_{this};
};

}  // namespace omnibox

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_POPUP_CLOSER_H_
