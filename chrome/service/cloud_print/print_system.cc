// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/print_system.h"

#include "base/guid.h"
#include "build/build_config.h"

namespace cloud_print {

PrintJobDetails::PrintJobDetails()
    : status(PRINT_JOB_STATUS_INVALID),
      platform_status_flags(0),
      total_pages(0),
      pages_printed(0) {
}

void PrintJobDetails::Clear() {
  status = PRINT_JOB_STATUS_INVALID;
  platform_status_flags = 0;
  status_message.clear();
  total_pages = 0;
  pages_printed = 0;
}

PrintSystem::PrintServerWatcher::~PrintServerWatcher() {}

PrintSystem::PrinterWatcher::~PrinterWatcher() {}

PrintSystem::JobSpooler::~JobSpooler() {}

PrintSystem::~PrintSystem() {}

std::string PrintSystem::GenerateProxyId() {
  return base::GenerateGUID();
}

#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && !defined(USE_CUPS)
scoped_refptr<PrintSystem> PrintSystem::CreateInstance(
    const base::DictionaryValue*) {
  return nullptr;
}
#endif

}  // namespace cloud_print
