// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_

#include <string>

#include "ui/views/view.h"

// Badge used to signify borealis' beta-ness on various UI surfaces.
class BorealisBetaBadge : public views::View {
  METADATA_HEADER(BorealisBetaBadge, views::View)

 public:

  BorealisBetaBadge();
  ~BorealisBetaBadge() override;

  // Not copyable or movable.
  BorealisBetaBadge(const BorealisBetaBadge&) = delete;
  BorealisBetaBadge& operator=(const BorealisBetaBadge&) = delete;

  std::u16string GetText() const;

  // View overrides.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& /*available_size*/) const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_
