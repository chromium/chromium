// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Chromium.

#include "chrome/install_static/chromium_install_modes.h"

#include <stdlib.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"";

const wchar_t kProductPathName[] = L"Chromium";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = "chromium";

const InstallConstants kInstallModes[] = {
    // The primary (and only) install mode for Chromium.
    {
        sizeof(kInstallModes[0]),
        CHROMIUM_INDEX,  // The one and only mode for Chromium.
        "",              // No install switch for the primary install mode.
        L"",             // Empty install_suffix for the primary install mode.
        L"",             // No logo suffix for the primary install mode.
        L"",          // Empty app_guid since no integraion with Google Update.
        L"Chromium",  // A distinct base_app_name.
        L"Chromium",  // A distinct base_app_id.
        L"ChromiumHTM",                             // ProgID prefix.
        L"Chromium HTML Document",                  // ProgID description.
        L"{7D2B3E1D-D096-4594-9D8F-A6667F12E0AC}",  // Active Setup GUID.
        L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}",  // CommandExecuteImpl CLSID.
        {0x635EFA6F,
         0x08D6,
         0x4EC9,
         {0xBD, 0x14, 0x8A, 0x0F, 0xDE, 0x97, 0x51,
          0x59}},  // Toast Activator CLSID.
        {0xD133B120,
         0x6DB4,
         0x4D6B,
         {0x8B, 0xFE, 0x83, 0xBF, 0x8C, 0xA1, 0xB1, 0xB0}},  // Elevator CLSID.

        {0xb88c45b9,
         0x8825,
         0x4629,
         {0xb8, 0x3e, 0x77, 0xcc, 0x67, 0xd9, 0xce,
          0xed}},  // IElevator IID and TypeLib
                   // {B88C45B9-8825-4629-B83E-77CC67D9CEED}.
        L"",       // Empty default channel name since no update integration.
        ChannelStrategy::UNSUPPORTED,
        true,   // Supports system-level installs.
        true,   // Supports in-product set as default browser UX.
        false,  // Does not support retention experiments.
        icon_resources::kApplicationIndex,  // App icon resource index.
        IDR_MAINFRAME,                      // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012148-",  // App container sid prefix for sandbox.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
