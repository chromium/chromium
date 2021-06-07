// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/arc_util_test_support.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/threading/thread_restrictions.h"

namespace arc {

bool GetSystemMemoryInfoForTesting(const std::string& file_name,
                                   base::SystemMemoryInfoKB* mem_info) {
  base::FilePath base_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &base_path);
  const base::FilePath test_path = base_path.Append("components")
                                       .Append("test")
                                       .Append("data")
                                       .Append("arc_mem_profile")
                                       .Append(file_name);
  base::ScopedAllowBlockingForTesting allowBlocking;
  std::string mem_info_data;
  return base::ReadFileToString(test_path, &mem_info_data) &&
         base::ParseProcMeminfo(mem_info_data, mem_info);
}

}  // namespace arc
