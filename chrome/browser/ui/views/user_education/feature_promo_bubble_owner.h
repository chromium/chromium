// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_H_

#include "base/callback_forward.h"
#include "base/token.h"
#include "chrome/browser/ui/views/user_education/feature_promo_bubble_view.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"

// Manages display of a user education bubble. Notifies a client when the bubble
// is closed. Ensures only one bubble shows per instance. This is an interface
// to enable more robust testing.
class FeaturePromoBubbleOwner {
 public:
  FeaturePromoBubbleOwner();
  virtual ~FeaturePromoBubbleOwner();

  FeaturePromoBubbleOwner(const FeaturePromoBubbleOwner&) = delete;
  FeaturePromoBubbleOwner& operator=(const FeaturePromoBubbleOwner&) = delete;

  // Show a bubble with `params`. Calls `close_callback` when the bubble is
  // closed, either by `CloseBubble()` or by the user. Returns a token that
  // identifies the bubble for CloseBubble or BubbleIsShowing calls. Fails and
  // returns nothing if a bubble is currently showing or the bubble couldn't be
  // created for other reasons.
  virtual absl::optional<base::Token> ShowBubble(
      FeaturePromoBubbleView::CreateParams params,
      base::OnceClosure close_callback) = 0;

  // Returns whether the bubble identified by `bubble_id` is still showing.
  virtual bool BubbleIsShowing(base::Token bubble_id) const = 0;

  // Returns whether any bubble is currently showing.
  virtual bool AnyBubbleIsShowing() const = 0;

  // Close the identified bubble, if one is showing.
  virtual void CloseBubble(base::Token bubble_id) = 0;

  // If a bubble is showing, updates its anchor position.
  virtual void NotifyAnchorBoundsChanged() = 0;

  // Gets the screen bounds of the given bubble, which must be showing.
  virtual gfx::Rect GetBubbleBoundsInScreen(base::Token bubble_id) const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_FEATURE_PROMO_BUBBLE_OWNER_H_
