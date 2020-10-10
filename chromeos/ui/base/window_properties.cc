// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/base/window_properties.h"

#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/window.h"

namespace chromeos {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kIsShowingInOverviewKey, false)

DEFINE_UI_CLASS_PROPERTY_KEY(WindowStateType,
                             kWindowStateTypeKey,
                             WindowStateType::kDefault)
}  // namespace chromeos
