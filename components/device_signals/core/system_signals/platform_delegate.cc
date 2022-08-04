// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/system_signals/platform_delegate.h"

#include "base/files/file_path.h"
#include "build/build_config.h"

namespace device_signals {

bool CustomFilePathComparator::operator()(const base::FilePath& a,
                                          const base::FilePath& b) const {
#if BUILDFLAG(IS_LINUX)
  // On Linux, the file system is case sensitive.
  return a < b;
#else
  // On Windows and Mac, the file system is case insensitive.
  return base::FilePath::CompareLessIgnoreCase(a.value(), b.value());
#endif
}

}  // namespace device_signals
