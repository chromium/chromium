// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_UTIL_IMPL_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_UTIL_IMPL_H_

#include "ash/public/cpp/oobe_dialog_util.h"

namespace ash {

class OobeDialogUtilImpl : public OobeDialogUtil {
 public:
  OobeDialogUtilImpl();
  OobeDialogUtilImpl(const OobeDialogUtilImpl&) = delete;
  OobeDialogUtilImpl& operator=(const OobeDialogUtilImpl&) = delete;
  ~OobeDialogUtilImpl() override;

  // `OobeDialogUtil` implementation:
  int GetShadowElevation() override;
  int GetCornerRadius() override;

  gfx::Size CalculateDialogSize(const gfx::Size& host_size,
                                int shelf_height,
                                bool is_horizontal) override;
};
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_OOBE_DIALOG_UTIL_IMPL_H_
