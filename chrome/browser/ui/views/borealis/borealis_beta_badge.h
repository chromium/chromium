// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_
#define CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_

#include <string>

#include "ui/views/view.h"
#include "ui/views/views_export.h"

namespace views {

// Badge used to signify borealis' beta-ness on various UI surfaces.
class VIEWS_EXPORT BorealisBetaBadge : public View {
 public:
  METADATA_HEADER(BorealisBetaBadge);

  BorealisBetaBadge();
  ~BorealisBetaBadge() override;

  // Not copyable or movable.
  BorealisBetaBadge(const BorealisBetaBadge&) = delete;
  BorealisBetaBadge& operator=(const BorealisBetaBadge&) = delete;

  std::u16string GetText() const;

  // View overrides.
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_BOREALIS_BOREALIS_BETA_BADGE_H_
