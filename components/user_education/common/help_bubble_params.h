// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_
#define COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
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
  HelpBubbleButtonParams(HelpBubbleButtonParams&&) noexcept;
  HelpBubbleButtonParams& operator=(HelpBubbleButtonParams&&) noexcept;
  ~HelpBubbleButtonParams();

  std::u16string text;
  bool is_default = false;
  base::OnceClosure callback = base::DoNothing();
};

struct HelpBubbleParams {
  // Platform-specific properties that can be set for a help bubble. If an
  // extended property evolves to warrant cross-platform support, it should be
  // promoted out of extended properties.
  class ExtendedProperties {
   public:
    ExtendedProperties();
    ExtendedProperties(const ExtendedProperties&);
    ExtendedProperties(ExtendedProperties&&) noexcept;
    ExtendedProperties& operator=(const ExtendedProperties&);
    ExtendedProperties& operator=(ExtendedProperties&&) noexcept;
    ~ExtendedProperties();

    bool operator==(const ExtendedProperties&) const;
    bool operator!=(const ExtendedProperties&) const;

    base::Value::Dict& values() { return dict_; }
    const base::Value::Dict& values() const { return dict_; }

   private:
    base::Value::Dict dict_;
  };

  HelpBubbleParams();
  HelpBubbleParams(HelpBubbleParams&&) noexcept;
  HelpBubbleParams& operator=(HelpBubbleParams&&) noexcept;
  ~HelpBubbleParams();

  HelpBubbleArrow arrow = HelpBubbleArrow::kTopRight;

  std::u16string title_text;
  raw_ptr<const gfx::VectorIcon> body_icon = nullptr;
  std::u16string body_icon_alt_text;
  std::u16string body_text;
  std::u16string screenreader_text;

  // Whether the bubble should receive focus when it is shown. This is a
  // behavioral hint; how it is actually implemented will depend on the bubble
  // implementation (for example, bubbles attached to menu items cannot take
  // focus for system activation reasons).
  std::optional<bool> focus_on_show_hint;

  // Additional message to be read to screen reader users to aid in
  // navigation.
  std::u16string keyboard_navigation_hint;

  // The buttons to display. Depending on platform defaults, a
  // HelpBubbleFactory may choose to move a default button to the leading or
  // trailing edge of the bubble; however the order of non-default buttons is
  // guaranteed to remain stable.
  std::vector<HelpBubbleButtonParams> buttons;

  // Alt text to use for the close button.
  std::u16string close_button_alt_text;

  // Determines whether a progress indicator will be displayed; if set the
  // first value is current progress and the second is max progress.
  std::optional<std::pair<int, int>> progress;

  // Sets the bubble timeout. If a timeout is not provided a default will
  // be used. If the timeout is 0, the bubble never times out.
  std::optional<base::TimeDelta> timeout;

  // Called when the bubble is actively dismissed by the user, using the close
  // button or the ESC key.
  base::OnceClosure dismiss_callback = base::DoNothing();

  // Called when the bubble times out.
  base::OnceClosure timeout_callback = base::DoNothing();

  // Platform-specific properties that can be set for a help bubble. If an
  // extended property evolves to warrant cross-platform support, it should be
  // promoted out of extended properties.
  ExtendedProperties extended_properties;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_HELP_BUBBLE_PARAMS_H_
