// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/vector_icon_types.h"

// Describes the content and appearance of an in-product help bubble.
// |body_string_specifier| and |arrow| are required, all other fields have good
// defaults. The anchor UI element is supplied in the framework-specific
// factory. For consistency between different in-product help flows, avoid
// changing more fields than necessary.
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
  absl::optional<int> title_string_specifier;

  // String to be announced when bubble is shown. Optional.
  absl::optional<int> screenreader_string_specifier;

  // The icon to display in the bubble next to the text.
  const gfx::VectorIcon* body_icon = nullptr;

  // A keyboard accelerator to access the feature. If
  // |screenreader_string_specifier| is set and contains a placeholder,
  // this is filled in and announced to the user.
  //
  // One of |feature_accelerator| or |feature_command_id|, or neither,
  // can be filled in. If |feature_command_id| is specified this ID is
  // looked up on BrowserView and the associated accelerator is fetched.
  absl::optional<ui::Accelerator> feature_accelerator;
  absl::optional<int> feature_command_id;

  // Positioning and sizing:

  // Mirrors most values of views::BubbleBorder::Arrow
  enum class Arrow {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    LEFT_TOP,
    RIGHT_TOP,
    LEFT_BOTTOM,
    RIGHT_BOTTOM,
    TOP_CENTER,
    BOTTOM_CENTER,
    LEFT_CENTER,
    RIGHT_CENTER,
    NONE
  };

  // Determines position relative to the anchor element, and where the bubble's
  // arrow points. Required.
  Arrow arrow = Arrow::TOP_LEFT;

  // If set, determines the width of the bubble. Prefer the default if
  // possible.
  absl::optional<int> preferred_width;

  // Determines if the bubble will get focused on creation.
  bool focus_on_create = false;

  // Determines if this bubble will be dismissed when it loses focus.
  // Only meaningful when |focus_on_create| is true. If it's false then it
  // starts out blurred.
  bool persist_on_blur = true;

  // Determines if this IPH can be snoozed and reactivated later.
  // If true, |allow_focus| must be true for keyboard accessibility.
  bool allow_snooze = false;

  // Changes the bubble timeout. If a timeout is not provided a default will
  // be used. If the timeout is 0, the bubble never times out.
  absl::optional<base::TimeDelta> timeout;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_FEATURE_PROMO_BUBBLE_PARAMS_H_
