// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_AUTH_PANEL_VIEWS_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_AUTH_PANEL_VIEWS_UTILS_H_

namespace ash {

class SystemTextfield;

// This function configures the `textfield` to be aethetically fitting for
// inputing text-based knowledge auth factors.
void ConfigureAuthTextField(SystemTextfield* textfield);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_AUTH_PANEL_VIEWS_AUTH_PANEL_VIEWS_UTILS_H_
