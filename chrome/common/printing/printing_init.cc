// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/printing/printing_init.h"

#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && BUILDFLAG(IS_WIN)
#include "chrome/common/printing/printer_capabilities.h"
#include "printing/backend/win_helper.h"
#endif

namespace printing {

void InitializeProcessForPrinting() {
#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && BUILDFLAG(IS_WIN)
  SetGetDisplayNameFunction(&GetUserFriendlyName);
#endif
}

}  // namespace printing
