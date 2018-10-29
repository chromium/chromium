// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar_host_unittest_util.h"

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "ui/base/ui_base_features.h"

namespace chrome {

void DisableFindBarAnimationsDuringTesting(bool disable) {
  DropdownBarHost::disable_animations_during_testing_ = disable;
}

}  // namespace chrome
