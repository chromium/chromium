// Copyright 2016 The Chromium Authors
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
        .size = sizeof(kInstallModes[0]),
        .index = CHROMIUM_INDEX,  // The one and only mode for Chromium.
        .install_switch =
            "",  // No install switch for the primary install mode.
        .install_suffix =
            L"",  // Empty install_suffix for the primary install mode.
        .logo_suffix = L"",  // No logo suffix for the primary install mode.
        .app_guid =
            L"",  // Empty app_guid since no integration with Google Update.
        .base_app_name = L"Chromium",              // A distinct base_app_name.
        .base_app_id = L"Chromium",                // A distinct base_app_id.
        .browser_prog_id_prefix = L"ChromiumHTM",  // Browser ProgID prefix.
        .browser_prog_id_description =
            L"Chromium HTML Document",         // Browser ProgID description.
        .pdf_prog_id_prefix = L"ChromiumPDF",  // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Chromium PDF Document",  // PDF ProgID description.
        .active_setup_guid =
            L"{7D2B3E1D-D096-4594-9D8F-A6667F12E0AC}",  // Active Setup GUID.
        .legacy_command_execute_clsid =
            L"{A2DF06F9-A21A-44A8-8A99-8B9C84F29160}",  // CommandExecuteImpl
                                                        // CLSID.
        .toast_activator_clsid = {0x635EFA6F,
                                  0x08D6,
                                  0x4EC9,
                                  {0xBD, 0x14, 0x8A, 0x0F, 0xDE, 0x97, 0x51,
                                   0x59}},  // Toast Activator CLSID.
        .elevator_clsid = {0xD133B120,
                           0x6DB4,
                           0x4D6B,
                           {0x8B, 0xFE, 0x83, 0xBF, 0x8C, 0xA1, 0xB1,
                            0xB0}},  // Elevator CLSID.
        .elevator_iid = {0xb88c45b9,
                         0x8825,
                         0x4629,
                         {0xb8, 0x3e, 0x77, 0xcc, 0x67, 0xd9, 0xce,
                          0xed}},  // IElevator IID and TypeLib
                                   // {B88C45B9-8825-4629-B83E-77CC67D9CEED}.
        .tracing_service_clsid = {0x83f69367,
                                  0x442d,
                                  0x447f,
                                  {0x8b, 0xcc, 0x0e, 0x3f, 0x97, 0xbe, 0x9c,
                                   0xf2}},  // SystemTraceSession CLSID.
        .tracing_service_iid = {0xa3fd580a,
                                0xffd4,
                                0x4075,
                                {0x91, 0x74, 0x75, 0xd0, 0xb1, 0x99, 0xd3,
                                 0xcb}},  // ISystemTraceSessionChromium IID and
                                          // TypeLib
        .default_channel_name =
            L"",  // Empty default channel name since no update integration.
        .channel_strategy = ChannelStrategy::UNSUPPORTED,
        .supports_system_level = true,  // Supports system-level installs.
        .supports_set_as_default_browser =
            true,  // Supports in-product set as default browser UX.
        .app_icon_resource_index =
            icon_resources::kApplicationIndex,  // App icon resource index.
        .app_icon_resource_id = IDR_MAINFRAME,  // App icon resource id.
        .html_doc_icon_resource_index =
            icon_resources::kHtmlDocIndex,  // HTML doc icon resource index.
        .pdf_doc_icon_resource_index =
            icon_resources::kPDFDocIndex,  // PDF doc icon resource index.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"924012148-",  // App container sid prefix for sandbox.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
