// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RECTF_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RECTF_H_

#include <ostream>

namespace autofill_assistant {

// A simple rectangle structure that uses float. Modelled on Android's RectF
// class.
struct RectF {
  float left;
  float top;
  float right;
  float bottom;
  bool full_width;

  RectF();
  RectF(float left, float top, float right, float bottom);
  RectF(float left, float top, float right, float bottom, bool full_width);

  // Checks whether the rectangle is empty.
  bool empty() const;

  // Intended for logging/debugging.
  friend std::ostream& operator<<(std::ostream& out, const RectF& rect);

  bool operator==(const RectF& another) const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_RECTF_H_
