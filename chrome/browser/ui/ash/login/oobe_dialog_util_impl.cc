// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/oobe_dialog_util_impl.h"

#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"

namespace ash {

OobeDialogUtilImpl::OobeDialogUtilImpl() = default;
OobeDialogUtilImpl::~OobeDialogUtilImpl() = default;

int OobeDialogUtilImpl::GetShadowElevation() {
  return kOobeDialogShadowElevation;
}

int OobeDialogUtilImpl::GetCornerRadius() {
  return kOobeDialogCornerRadius;
}

gfx::Size OobeDialogUtilImpl::CalculateDialogSize(const gfx::Size& host_size,
                                                  int shelf_height,
                                                  bool is_horizontal) {
  return CalculateOobeDialogSize(host_size, shelf_height, is_horizontal);
}

}  // namespace ash
