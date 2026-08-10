// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_FULL_WEBUI_OMNIBOX_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_FULL_WEBUI_OMNIBOX_FRAME_H_

#include "chrome/browser/ui/views/omnibox/rounded_omnibox_results_frame.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/insets.h"

class LocationBar;
namespace views {
class View;
}

class FullWebUIOmniboxFrame : public RoundedOmniboxResultsFrame {
  METADATA_HEADER(FullWebUIOmniboxFrame, RoundedOmniboxResultsFrame)

 public:
  FullWebUIOmniboxFrame(views::View* contents,
                        LocationBar* location_bar,
                        bool forward_mouse_events);
  FullWebUIOmniboxFrame(const FullWebUIOmniboxFrame&) = delete;
  FullWebUIOmniboxFrame& operator=(const FullWebUIOmniboxFrame&) = delete;
  ~FullWebUIOmniboxFrame() override;

  void SetElevation(int elevation);

  // Updates whether mouse events should be forwarded to the underlying
  // location bar.
  void SetForwardMouseEvents(bool forward);

  // views::View:
  void AddedToWidget() override;
#if !defined(USE_AURA)
  void OnMouseEvent(ui::MouseEvent* event) override;
#endif  // !USE_AURA

 private:
  gfx::Insets GetEventForwardingInsets();
#if defined(USE_AURA)
  void UpdateWindowTargeter();
#endif  // USE_AURA
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_FULL_WEBUI_OMNIBOX_FRAME_H_
