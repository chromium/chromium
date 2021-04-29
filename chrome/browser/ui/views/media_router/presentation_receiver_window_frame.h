// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace ui {
class ThemeProvider;
}

class Profile;

namespace views {
class WidgetDelegateView;
}

// This class implements the window portion of PresentationReceiverWindow.  It
// is just a normal Widget that overrides GetThemeProvider to provide normal
// colors for the URL bar.
class PresentationReceiverWindowFrame final : public views::Widget {
 public:
  explicit PresentationReceiverWindowFrame(Profile* profile);
  ~PresentationReceiverWindowFrame() final;

  void InitReceiverFrame(std::unique_ptr<views::WidgetDelegateView> delegate,
                         const gfx::Rect& bounds);

 private:
  const ui::ThemeProvider* GetThemeProvider() const final;

  // The profile from which we get the theme.
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(PresentationReceiverWindowFrame);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_
