// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_coordinator.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"

MediaCoordinator::MediaCoordinator(ViewType view_type,
                                   views::View& parent_view,
                                   std::optional<size_t> index,
                                   bool is_subsection) {
  auto* media_view =
      parent_view.AddChildViewAt(std::make_unique<MediaView>(is_subsection),
                                 index.value_or(parent_view.children().size()));

  if (!is_subsection) {
    auto* provider = ChromeLayoutProvider::Get();
    const int kRoundedRadius = provider->GetCornerRadiusMetric(
        views::ShapeContextTokens::kOmniboxExpandedRadius);
    const int kBorderThickness =
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);

    media_view->SetBorder(views::CreateThemedRoundedRectBorder(
        kBorderThickness, kRoundedRadius, ui::kColorButtonBorder));
    media_view->SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorButtonBorder, kRoundedRadius));
  }

  if (view_type != ViewType::kMicOnly) {
    camera_coordinator_.emplace(*media_view, /*needs_borders=*/!is_subsection);
  }

  if (view_type != ViewType::kCameraOnly) {
    mic_coordinator_.emplace(*media_view, /*needs_borders=*/!is_subsection);
  }
}

MediaCoordinator::~MediaCoordinator() = default;
