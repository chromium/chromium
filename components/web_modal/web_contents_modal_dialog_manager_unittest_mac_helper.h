// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_UNITTEST_MAC_HELPER_H_
#define COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_UNITTEST_MAC_HELPER_H_

#include "ui/gfx/native_widget_types.h"

// Returns a fake gfx::NativeWindow for testing purposes. This is a placeholder,
// only usable for comparisons. Any attempt to actually use it will likely
// crash.
gfx::NativeWindow FakeNativeWindowForTesting();

// Tears down the fake windows created by `FakeNativeWindowForTesting()`, above.
// (This is required because gfx::NativeWindow is a weak reference to an object,
// so there must be something else keeping those objects alive, and this cleans
// out that strong reference.)
void TearDownFakeNativeWindowsForTesting();

#endif  // COMPONENTS_WEB_MODAL_WEB_CONTENTS_MODAL_DIALOG_MANAGER_UNITTEST_MAC_HELPER_H_
