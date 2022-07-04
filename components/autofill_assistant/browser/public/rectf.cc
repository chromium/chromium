// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/rectf.h"

namespace autofill_assistant {

RectF::RectF()
    : left(0.0f), top(0.0f), right(0.0f), bottom(0.0f), full_width(false) {}
RectF::RectF(float l, float t, float r, float b)
    : left(l), top(t), right(r), bottom(b), full_width(false) {}
RectF::RectF(float l, float t, float r, float b, bool fw)
    : left(l), top(t), right(r), bottom(b), full_width(fw) {}

bool RectF::empty() const {
  return (!full_width && right <= left) || bottom <= top;
}

bool RectF::operator==(const RectF& another) const {
  return left == another.left && top == another.top && right == another.right &&
         bottom == another.bottom && full_width == another.full_width;
}

std::ostream& operator<<(std::ostream& out, const RectF& rect) {
  out << "[l: " << rect.left << ", t: " << rect.top << ", r: " << rect.right
      << ", b: " << rect.bottom << "]";
  if (rect.full_width) {
    out << " full width";
  }
  return out;
}

}  // namespace autofill_assistant
