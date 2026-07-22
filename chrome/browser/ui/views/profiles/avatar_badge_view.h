// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BADGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BADGE_VIEW_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/ui/profiles/avatar_badge_types.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// A custom-painted profile avatar badge with a given text label and optional
// background wave path.
class AvatarBadgeView : public views::View {
  METADATA_HEADER(AvatarBadgeView, views::View)

 public:
  // Returns the localized badge label string for the given subscription tier,
  // or an empty string if the tier is not recognized.
  static std::u16string GetAvatarBadgeLabel(int tier);

  // Constructs an AvatarBadgeView with the specified `label_text` and optional
  // decorative `wave_path`.
  explicit AvatarBadgeView(const std::u16string& label_text,
                           std::optional<SkPath> wave_path = std::nullopt);
  AvatarBadgeView(const AvatarBadgeView&) = delete;
  AvatarBadgeView& operator=(const AvatarBadgeView&) = delete;
  ~AvatarBadgeView() override;

  // views::View:
  // Calculates the preferred size based on label text dimensions and padding.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  // Custom paints the badge background pill, wave pattern, inner shadow, and
  // text label.
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  const std::u16string label_text_;
  const std::optional<SkPath> wave_path_;
};

// Factory method that validates specifications and returns a new
// AvatarBadgeView instance representing a pill-shaped badge with the given
// `label`.
// Returns nullptr if `label` is empty, the feature is disabled, or if spec
// validation fails.
std::unique_ptr<views::View> GetAvatarBadgeView(const std::u16string& label);

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_AVATAR_BADGE_VIEW_H_
