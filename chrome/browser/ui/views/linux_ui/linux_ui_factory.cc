// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/linux_ui/linux_ui_factory.h"

namespace views {

LinuxUI* BuildLinuxUI() {
  // Users can use their own implementations of LinuxUI. By default, if the gtk
  // is not available, the LinuxUI is not used.
  return nullptr;
}

}  // namespace views
