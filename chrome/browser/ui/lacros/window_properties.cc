// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lacros/window_properties.h"

#include "chromeos/ui/base/window_pin_type.h"
#include "ui/base/class_property.h"

namespace lacros {

DEFINE_UI_CLASS_PROPERTY_KEY(chromeos::WindowPinType,
                             kWindowPinTypeKey,
                             chromeos::WindowPinType::kNone)

}  // namespace lacros
