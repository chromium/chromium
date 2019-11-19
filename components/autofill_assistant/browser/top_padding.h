// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TOP_PADDING_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TOP_PADDING_H_

namespace autofill_assistant {

// A simple structure that holds information about the top padding.
// This structure is used by WebController.FocusElement.
//
// Only one type of value can be set (pixels or ratio). If one is
// set, other returns 0.
struct TopPadding {
  enum class Unit {
    // Css Pixels.
    PIXELS = 0,
    // Ratio in relation to window.innerHeight.
    RATIO = 1
  };

  TopPadding();
  TopPadding(float value, Unit unit);

  // Returns 0 if value set in Ratio.
  float pixels() const;
  // Returns 0 if value set in CSS Pixels.
  float ratio() const;

 private:
  float value_ = 0;
  Unit unit_ = Unit::PIXELS;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TOP_PADDING_H_
