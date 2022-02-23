// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific constants and install modes for Google Chrome.

#include "chrome/install_static/google_chrome_install_modes.h"

#include <stdlib.h>

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/install_static/install_modes.h"

namespace install_static {

const wchar_t kCompanyPathName[] = L"Google";

const wchar_t kProductPathName[] = L"Chrome";

const size_t kProductPathNameLength = _countof(kProductPathName) - 1;

const char kSafeBrowsingName[] = "googlechrome";

const char kDeviceManagementServerHostName[] = "m.google.com";

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        sizeof(kInstallModes[0]),
        STABLE_INDEX,  // The first mode is for stable/beta/dev.
        "",            // No install switch for the primary install mode.
        L"",           // Empty install_suffix for the primary install mode.
        L"",           // No logo suffix for the primary install mode.
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",
        L"Google Chrome",                           // A distinct base_app_name.
        L"Chrome",                                  // A distinct base_app_id.
        L"ChromeHTML",                              // ProgID prefix.
        L"Chrome HTML Document",                    // ProgID description.
        L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",  // Active Setup GUID.
        L"{5C65F4B0-3651-4514-B207-D10CB699B14B}",  // CommandExecuteImpl CLSID.
        {0xA2C6CB58,
         0xC076,
         0x425C,
         {0xAC, 0xB7, 0x6D, 0x19, 0xD6, 0x44, 0x28,
          0xCD}},  // Toast Activator CLSID.
        {0x708860E0,
         0xF641,
         0x4611,
         {0x88, 0x95, 0x7D, 0x86, 0x7D, 0xD3, 0x67, 0x5B}},  // Elevator CLSID.

        {0x463abecf,
         0x410d,
         0x407f,
         {0x8a, 0xf5, 0xd, 0xf3, 0x5a, 0x0, 0x5c,
          0xc8}},  // IElevator IID and TypeLib
                   // {463ABECF-410D-407F-8AF5-0DF35A005CC8}.
        L"",       // The empty string means "stable".
        ChannelStrategy::FLOATING,
        true,  // Supports system-level installs.
        true,  // Supports in-product set as default browser UX.
        true,  // Supports retention experiments.
        icon_resources::kApplicationIndex,  // App icon resource index.
        IDR_MAINFRAME,                      // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012149-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome Beta
    {
        sizeof(kInstallModes[0]),
        BETA_INDEX,     // The mode for the side-by-side beta channel.
        "chrome-beta",  // Install switch.
        L" Beta",       // Install suffix.
        L"Beta",        // Logo suffix.
        L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // A distinct app GUID.
        L"Google Chrome Beta",                      // A distinct base_app_name.
        L"ChromeBeta",                              // A distinct base_app_id.
        L"ChromeBHTML",                             // ProgID prefix.
        L"Chrome Beta HTML Document",               // ProgID description.
        L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // Active Setup GUID.
        L"",                                        // CommandExecuteImpl CLSID.
        {0xB89B137F,
         0x96AA,
         0x4AE2,
         {0x98, 0xC4, 0x63, 0x73, 0xEA, 0xA1, 0xEA,
          0x4D}},  // Toast Activator CLSID.
        {0xDD2646BA,
         0x3707,
         0x4BF8,
         {0xB9, 0xA7, 0x3, 0x86, 0x91, 0xA6, 0x8F, 0xC2}},  // Elevator CLSID.

        {0xa2721d66,
         0x376e,
         0x4d2f,
         {0x9f, 0xf, 0x90, 0x70, 0xe9, 0xa4, 0x2b,
          0x5f}},  // IElevator IID and TypeLib
                   // {A2721D66-376E-4D2F-9F0F-9070E9A42B5F}.
        L"beta",   // Forced channel name.
        ChannelStrategy::FIXED,
        true,  // Supports system-level installs.
        true,  // Supports in-product set as default browser UX.
        true,  // Supports retention experiments.
        icon_resources::kBetaApplicationIndex,  // App icon resource index.
        IDR_X005_BETA,                          // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012151-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome Dev
    {
        sizeof(kInstallModes[0]),
        DEV_INDEX,     // The mode for the side-by-side dev channel.
        "chrome-dev",  // Install switch.
        L" Dev",       // Install suffix.
        L"Dev",        // Logo suffix.
        L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // A distinct app GUID.
        L"Google Chrome Dev",                       // A distinct base_app_name.
        L"ChromeDev",                               // A distinct base_app_id.
        L"ChromeDHTML",                             // ProgID prefix.
        L"Chrome Dev HTML Document",                // ProgID description.
        L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // Active Setup GUID.
        L"",                                        // CommandExecuteImpl CLSID.
        {0xF01C03EB,
         0xD431,
         0x4C83,
         {0x8D, 0x7A, 0x90, 0x27, 0x71, 0xE7, 0x32,
          0xFA}},  // Toast Activator CLSID.
        {0xDA7FDCA5,
         0x2CAA,
         0x4637,
         {0xAA, 0x17, 0x7, 0x40, 0x58, 0x4D, 0xE7, 0xDA}},  // Elevator CLSID.

        {0xbb2aa26b,
         0x343a,
         0x4072,
         {0x8b, 0x6f, 0x80, 0x55, 0x7b, 0x8c, 0xe5,
          0x71}},  // IElevator IID and TypeLib
                   // {BB2AA26B-343A-4072-8B6F-80557B8CE571}.
        L"dev",    // Forced channel name.
        ChannelStrategy::FIXED,
        true,  // Supports system-level installs.
        true,  // Supports in-product set as default browser UX.
        true,  // Supports retention experiments.
        icon_resources::kDevApplicationIndex,  // App icon resource index.
        IDR_X004_DEV,                          // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012152-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome SxS (canary).
    {
        sizeof(kInstallModes[0]),
        CANARY_INDEX,  // The mode for the side-by-side canary channel.
        "chrome-sxs",  // Install switch.
        L" SxS",       // Install suffix.
        L"Canary",     // Logo suffix.
        L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // A distinct app GUID.
        L"Google Chrome Canary",                    // A distinct base_app_name.
        L"ChromeCanary",                            // A distinct base_app_id.
        L"ChromeSSHTM",                             // ProgID prefix.
        L"Chrome Canary HTML Document",             // ProgID description.
        L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // Active Setup GUID.
        L"{1BEAC3E3-B852-44F4-B468-8906C062422E}",  // CommandExecuteImpl CLSID.
        {0xFA372A6E,
         0x149F,
         0x4E95,
         {0x83, 0x2D, 0x8F, 0x69, 0x8D, 0x40, 0xAD,
          0x7F}},  // Toast Activator CLSID.
        {0x704C2872,
         0x2049,
         0x435E,
         {0xA4, 0x69, 0xA, 0x53, 0x43, 0x13, 0xC4, 0x2B}},  // Elevator CLSID.

        {0x4f7ce041,
         0x28e9,
         0x484f,
         {0x9d, 0xd0, 0x61, 0xa8, 0xca, 0xce, 0xfe,
          0xe4}},   // IElevator IID and TypeLib
                    // {4F7CE041-28E9-484F-9DD0-61A8CACEFEE4}.
        L"canary",  // Forced channel name.
        ChannelStrategy::FIXED,
        false,  // Does not support system-level installs.
        false,  // Does not support in-product set as default browser UX.
        true,   // Supports retention experiments.
        icon_resources::kSxSApplicationIndex,  // App icon resource index.
        IDR_SXS,                               // App icon resource id.
        L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
        L"924012150-",  // App container sid prefix for sandbox.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
