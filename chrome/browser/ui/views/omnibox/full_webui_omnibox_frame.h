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

  // The shadow margin, with the top expanded to also cover the location bar.
  // Events inside these insets are forwarded to the browser window beneath.
  gfx::Insets GetEventForwardingInsets() const;

  // How the Full WebUI Widget is aligned relative to the location bar.
  static gfx::Insets GetLocationBarAlignmentInsets();

 private:
#if defined(USE_AURA)
  void UpdateWindowTargeter();
#endif  // USE_AURA
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_FULL_WEBUI_OMNIBOX_FRAME_H_
