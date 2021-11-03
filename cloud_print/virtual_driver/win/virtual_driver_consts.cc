// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// THIS IS A ZOMBIEFILE!!! RUN!!
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"

#include <windows.h>
#include <stddef.h>

#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"

namespace cloud_print {

const wchar_t kPortName[] = L"GCP:";
const size_t kPortNameSize = sizeof(kPortName);
const wchar_t kGoogleUpdateProductId[] =
    L"{9B13FA92-1F73-4761-AB78-2C6ADAC3660D}";

}  // namespace cloud_print
