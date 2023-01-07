// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/gfx/vector_icon_types.h"

namespace user_education {

// The amount of time the promo should stay onscreen.
constexpr base::TimeDelta kDefaultTimeoutWithoutButtons = base::Seconds(10);
constexpr base::TimeDelta kDefaultTimeoutWithButtons = base::Seconds(0);

// Mirrors most values of views::BubbleBorder::Arrow.
// All values except kNone show a visible arrow between the bubble and the
// anchor element.
enum class HelpBubbleArrow {
  kNone,  // Positions the bubble directly beneath the anchor with no arrow.
  kTopLeft,
  kTopRight,
  kBottomLeft,
  kBottomRight,
  kLeftTop,
  kRightTop,
  kLeftBottom,
  kRightBottom,
  kTopCenter,
  kBottomCenter,
  kLeftCenter,
  kRightCenter,
};

struct HelpBubbleButtonParams {
  HelpBubbleButtonParams();
  HelpBubbleButtonParams(HelpBubbleButtonParams&&);
  ~HelpBubbleButtonParams();
  HelpBubbleButtonParams& operator=(HelpBubbleButtonParams&&);

  std::u16string text;
  bool is_default = false;
  base::OnceClosure callback = base::DoNothing();
};

struct HelpBubbleParams {
  HelpBubbleParams();
  HelpBubbleParams(HelpBubbleParams&&);
  ~HelpBubbleParams();
  HelpBubbleParams& operator=(HelpBubbleParams&&);

  HelpBubbleArrow arrow = HelpBubbleArrow::kTopRight;

  std::u16string title_text;
  raw_ptr<const gfx::VectorIcon> body_icon = nullptr;
  std::u16string body_icon_alt_text;
  std::u16string body_text;
  std::u16string screenreader_text;

  // Additional message to be read to screen reader users to aid in
  // navigation.
  std::u16string keyboard_navigation_hint;

  // The buttons to display. Depending on platform defaults, a
  // HelpBubbleFactory may choose to move a default button to the leading or
  // trailing edge of the bubble; however the order of non-default buttons is
  // guaranteed to remain stable.
  std::vector<HelpBubbleButtonParams> buttons;

  // If set to true, a close button will always be shown.
  bool force_close_button = false;

  // Alt text to use for the close button.
  std::u16string close_button_alt_text;

  // Determines whether a progress indicator will be displayed; if set the
  // first value is current progress and the second is max progress.
  absl::optional<std::pair<int, int>> progress;

  // Sets the bubble timeout. If a timeout is not provided a default will
  // be used. If the timeout is 0, the bubble never times out.
  absl::optional<base::TimeDelta> timeout;

  // Called when the bubble is actively dismissed by the user, using the close
  // button or the ESC key.
  base::OnceClosure dismiss_callback = base::DoNothing();

  // Called when the bubble times out.
  base::OnceClosure timeout_callback = base::DoNothing();
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_
