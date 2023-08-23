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

// Flag for whether we ignore max_size hints for maximise purposes. Instead we
// use the logic that if we can resize, we can maximise. Should only be set to
// true for Sommelier-based windows.
extern const ui::ClassProperty<bool>* const kMaximumSizeForResizabilityOnly;

}  // namespace exo

#endif  // COMPONENTS_EXO_WINDOW_PROPERTIES_H_
