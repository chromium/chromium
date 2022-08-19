// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/chrome_glanceables_delegate.h"

#include "base/notreached.h"

ChromeGlanceablesDelegate::ChromeGlanceablesDelegate() = default;

ChromeGlanceablesDelegate::~ChromeGlanceablesDelegate() = default;

void ChromeGlanceablesDelegate::RestoreSession() {
  // TODO(crbug.com/1353119): Use the FullRestoreService to trigger session
  // restore.
  NOTIMPLEMENTED();
}
