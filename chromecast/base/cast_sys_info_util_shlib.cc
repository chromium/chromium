// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/public/cast_sys_info_shlib.h"
#include "chromecast/public/cast_sys_info.h"

namespace chromecast {

std::unique_ptr<CastSysInfo> CreateSysInfo() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return base::WrapUnique(CastSysInfoShlib::Create(command_line->argv()));
}

}  // namespace chromecast
