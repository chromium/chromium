// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/validation_delegate.h"

#include "base/notreached.h"

namespace payments {

ValidationDelegate::~ValidationDelegate() = default;

bool ValidationDelegate::ShouldFormat() {
  return false;
}

std::u16string ValidationDelegate::Format(std::u16string_view text) {
  NOTREACHED();
}

}  // namespace payments
