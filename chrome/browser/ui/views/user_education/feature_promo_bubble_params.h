// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_

#include <memory>
#include <string>

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

  // If |body_string_specifier| is not set, this will be used instead.
  // Only use if your string has placeholders that need to be filled in
  // dynamically.
  //
  // TODO(crbug.com/1143971): enable filling placeholders in
  // |body_string_specifier| with context-specific information then
  // remove this.
  std::u16string body_text_raw;

  // Title shown larger at top of bubble. Optional.
  base::Optional<int> title_string_specifier;

  // String to be announced when bubble is shown. Optional.
  base::Optional<int> screenreader_string_specifier;

  // A keyboard accelerator to access the feature. If
  // |screenreader_string_specifier| is set and contains a placeholder,
  // this is filled in and announced to the user.
  //
  // One of |feature_accelerator| or |feature_command_id|, or neither,
  // can be filled in. If |feature_command_id| is specified this ID is
  // looked up on BrowserView and the associated accelerator is fetched.
  base::Optional<ui::Accelerator> feature_accelerator;
  base::Optional<int> feature_command_id;

  // Positioning and sizing:

  // View bubble is positioned relative to. Required.
  views::View* anchor_view = nullptr;

  // Determines position relative to |anchor_view|. Required. Note
  // that contrary to the name, no visible arrow is shown.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_LEFT;

  // If set, determines the width of the bubble. Prefer the default if
  // possible.
  base::Optional<int> preferred_width;

  // Determines if this bubble can be focused. If true, it will get
  // focused on creation.
  bool allow_focus = false;

  // Determines if this bubble will be dismissed when it loses focus.
  // Only meaningful when |allow_focus| is true. When |allow_focus|
  // is false, the bubble will always persist because it will never
  // get blurred.
  bool persist_on_blur = false;

  // Determines if this IPH can be snoozed and reactivated later.
  // If true, |allow_focus| must be true for keyboard accessibility.
  bool allow_snooze = false;

  // Changes the bubble timeout. Intended for tests, avoid use.
  base::Optional<base::TimeDelta> timeout_default;
  base::Optional<base::TimeDelta> timeout_short;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_
