// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/bubble_manager_impl.h"

namespace autofill {

BubbleManagerImpl::BubbleManagerImpl() = default;

BubbleManagerImpl::~BubbleManagerImpl() = default;

void BubbleManagerImpl::RequestShowController(
    BubbleControllerBase& controller_to_show) {
  // TODO(crbug.com/432429605): Implement.
}
void BubbleManagerImpl::OnBubbleHiddenByController(
    BubbleControllerBase& controller_to_hide) {
  // TODO(crbug.com/432429605): Implement.
}

}  // namespace autofill
