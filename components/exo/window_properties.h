// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WINDOW_PROPERTIES_H_
#define COMPONENTS_EXO_WINDOW_PROPERTIES_H_

#include <string>

#include "ui/base/class_property.h"

namespace exo {

// Application Id set by the client. For example:
// "org.chromium.arc.<task-id>" for ARC++ shell surfaces.
// "org.chromium.lacros.<window-id>" for Lacros browser shell surfaces.
extern const ui::ClassProperty<std::string*>* const kApplicationIdKey;

}  // namespace exo

#endif  // COMPONENTS_EXO_WINDOW_PROPERTIES_H_
