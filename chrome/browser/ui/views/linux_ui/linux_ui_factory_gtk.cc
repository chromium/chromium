// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/linux_ui/linux_ui_factory.h"

#include "chrome/browser/ui/libgtkui/gtk_ui.h"

namespace views {

LinuxUI* BuildLinuxUI() {
  return BuildGtkUi();
}

}  // namespace views
