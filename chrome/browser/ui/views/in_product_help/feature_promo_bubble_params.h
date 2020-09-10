// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_PARAMS_H_

#include <memory>

#include "base/optional.h"
#include "base/time/time.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/bubble/bubble_border.h"

// Describes the content and appearance of an in-product help bubble.
// |body_string_specifier|, |anchor_view|, and |arrow| are required, all
// other fields have good defaults. For consistency between different
// in-product help flows, avoid changing more fields than necessary.
struct FeaturePromoBubbleParams {
  FeaturePromoBubbleParams();
  ~FeaturePromoBubbleParams();

  FeaturePromoBubbleParams(const FeaturePromoBubbleParams&);

  // Promo contents:

  // The main promo text. Must be set to a valid string specifier.
  int body_string_specifier = -1;

  // Title shown larger at top of bubble. Optional.
  base::Optional<int> title_string_specifier;

  // String to be announced when bubble is shown. Optional.
  base::Optional<int> screenreader_string_specifier;

  // A keyboard accelerator to access the feature. If
  // |screenreader_string_specifier| is set and contains a
  // placeholder, this is filled in.
  base::Optional<ui::Accelerator> feature_accelerator;

  // Positioning and sizing:

  // View bubble is positioned relative to. Required.
  views::View* anchor_view = nullptr;

  // Determines position relative to |anchor_view|. Required. Note
  // that contrary to the name, no visible arrow is shown.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT;

  // If set, determines the width of the bubble. Prefer the default if
  // possible.
  base::Optional<int> preferred_width;

  enum class ActivationAction {
    DO_NOT_ACTIVATE,
    ACTIVATE,
  };

  // Determines whether the bubble's widget can be activated, and
  // activates it on creation if so.
  ActivationAction activation_action = ActivationAction::DO_NOT_ACTIVATE;

  // Changes the bubble timeout. Intended for tests, avoid use.
  base::Optional<base::TimeDelta> timeout_default;
  base::Optional<base::TimeDelta> timeout_short;

  // Determines if this IPH can be snoozed and reactivated later.
  bool allow_snooze = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_IN_PRODUCT_HELP_FEATURE_PROMO_BUBBLE_PARAMS_H_
