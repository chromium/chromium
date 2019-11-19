// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/top_padding.h"

namespace autofill_assistant {

TopPadding::TopPadding() {}

TopPadding::TopPadding(float val, Unit u) : value_(val), unit_(u) {}

float TopPadding::pixels() const {
  if (unit_ == TopPadding::Unit::PIXELS) {
    return value_;
  }
  return 0;
}

float TopPadding::ratio() const {
  if (unit_ == TopPadding::Unit::RATIO) {
    return value_;
  }
  return 0;
}

}  // namespace autofill_assistant
