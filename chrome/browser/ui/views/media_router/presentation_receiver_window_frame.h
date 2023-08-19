// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_

#include <memory>

#include "base/memory/raw_ptr.h"
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

  PresentationReceiverWindowFrame(const PresentationReceiverWindowFrame&) =
      delete;
  PresentationReceiverWindowFrame& operator=(
      const PresentationReceiverWindowFrame&) = delete;

  ~PresentationReceiverWindowFrame() final;

  void InitReceiverFrame(std::unique_ptr<views::WidgetDelegateView> delegate,
                         const gfx::Rect& bounds);

 private:
  const ui::ThemeProvider* GetThemeProvider() const final;
  ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme() const final;

  // The profile from which we get the theme.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_PRESENTATION_RECEIVER_WINDOW_FRAME_H_
