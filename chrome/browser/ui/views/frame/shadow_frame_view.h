// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_FRAME_VIEW_H_

#include <memory>
#include <optional>

#include "ui/views/view.h"

namespace views {
class ViewShadow;
}

// Non-interactive child view that can be used to add a TopChrome-specific
// shadow to an elevated element. This view should be laid out in whatever
// bounds you want the shadow box to be around.
class ShadowFrameView : public views::View {
  METADATA_HEADER(ShadowFrameView, views::View)

 public:
  // Describes the amount of shadow the view should cast; all colors are black
  // with the given alpha; opacity is applied on top of this.
  struct ShadowAlpha {
    double light_key = 0.0;
    double light_ambient = 0.0;
    double dark_key = 0.0;
    double dark_ambient = 0.0;
  };

  ShadowFrameView(int elevation, ShadowAlpha alpha);
  ~ShadowFrameView() override;

  // Sets the shadow to be visible.
  void SetShadowVisible(bool visible);

  // Sets the shadow opacity.
  void SetShadowOpacity(double opacity);

  // Sets the shadow corner radius.
  void SetShadowCornerRadius(int corner_radius);

 protected:
  // views::View:
  void OnThemeChanged() override;

 private:
  void UpdateShadowColors();

  const int shadow_elevation_;
  const ShadowAlpha shadow_alpha_;

  // This default configuration is designed to have broadly-acceptable values,
  // but should be changed to specific values when shipping a feature using this
  // class.
  int corner_radius_ = 8;
  double shadow_opacity_ = 1.0;
  std::optional<bool> was_dark_;

  std::unique_ptr<views::ViewShadow> view_shadow_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SHADOW_FRAME_VIEW_H_
