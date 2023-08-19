// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view_test_api.h"

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"

namespace chromeos {

MultitaskMenuViewTestApi::MultitaskMenuViewTestApi(MultitaskMenuView* view)
    : multitask_menu_view_(view) {}

MultitaskMenuViewTestApi::~MultitaskMenuViewTestApi() = default;

SplitButtonView* MultitaskMenuViewTestApi::GetHalfButton() {
  return multitask_menu_view_->half_button_;
}

MultitaskButton* MultitaskMenuViewTestApi::GetFullButton() {
  return multitask_menu_view_->full_button_;
}

MultitaskButton* MultitaskMenuViewTestApi::GetFloatButton() {
  return multitask_menu_view_->float_button_;
}

bool MultitaskMenuViewTestApi::GetIsReversed() const {
  return multitask_menu_view_->is_reversed_;
}

}  // namespace chromeos
