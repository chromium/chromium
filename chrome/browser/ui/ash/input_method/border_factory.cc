// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/input_method/border_factory.h"

#include "ui/views/layout/layout_provider.h"

namespace ui {
namespace ime {

std::unique_ptr<views::BubbleBorder> GetBorderForWindow(
    WindowBorderType windowType) {
  std::unique_ptr<views::BubbleBorder> border;
  switch (windowType) {
    // Currently all cases are the same, but they may become different later.
    case WindowBorderType::Undo:
    case WindowBorderType::Suggestion:
    default:
      border = std::make_unique<views::BubbleBorder>(
          views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW);
      border->set_md_shadow_elevation(
          views::LayoutProvider::Get()->GetShadowElevationMetric(
              views::Emphasis::kMedium));
  }
  border->SetCornerRadius(views::LayoutProvider::Get()->GetCornerRadiusMetric(
      views::Emphasis::kMedium));
  return border;
}

}  // namespace ime
}  // namespace ui
