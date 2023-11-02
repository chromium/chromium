// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WINDOW_PROPERTIES_H_
#define COMPONENTS_EXO_WINDOW_PROPERTIES_H_

#include <string>

#include "components/exo/protected_native_pixmap_query_delegate.h"
#include "ui/base/class_property.h"

namespace exo {

// Application Id set by the client. For example:
// "org.chromium.arc.<task-id>" for ARC++ shell surfaces.
// "org.chromium.lacros.<window-id>" for Lacros browser shell surfaces.
extern const ui::ClassProperty<std::string*>* const kApplicationIdKey;

// Whether Restore and Maximize should exit full screen for this window.
// Currently only set to true for Lacros windows.
extern const ui::ClassProperty<bool>* const kRestoreOrMaximizeExitsFullscreen;

// Provides access to a delegate for determining if a native pixmap corresponds
// to a HW protected buffer.
extern const ui::ClassProperty<ProtectedNativePixmapQueryDelegate*>* const
    kProtectedNativePixmapQueryDelegate;
}  // namespace exo

#endif  // COMPONENTS_EXO_WINDOW_PROPERTIES_H_
