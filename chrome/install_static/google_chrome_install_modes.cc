// Copyright 2016 The Chromium Authors
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

const InstallConstants kInstallModes[] = {
    // The primary install mode for stable Google Chrome.
    {
        .size = sizeof(kInstallModes[0]),
        .index = STABLE_INDEX,  // The first mode is for stable/beta/dev.
        .install_switch =
            "",  // No install switch for the primary install mode.
        .install_suffix =
            L"",  // Empty install_suffix for the primary install mode.
        .logo_suffix = L"",  // No logo suffix for the primary install mode.
        .app_guid = L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",
        .base_app_name = L"Google Chrome",        // A distinct base_app_name.
        .base_app_id = L"Chrome",                 // A distinct base_app_id.
        .browser_prog_id_prefix = L"ChromeHTML",  // Browser ProgID prefix.
        .browser_prog_id_description =
            L"Chrome HTML Document",         // Browser ProgID description.
        .pdf_prog_id_prefix = L"ChromePDF",  // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Chrome PDF Document",  // PDF ProgID description.
        .active_setup_guid =
            L"{8A69D345-D564-463c-AFF1-A69D9E530F96}",  // Active Setup GUID.
        .legacy_command_execute_clsid =
            L"{5C65F4B0-3651-4514-B207-D10CB699B14B}",  // CommandExecuteImpl
                                                        // CLSID.
        .toast_activator_clsid = {0xA2C6CB58,
                                  0xC076,
                                  0x425C,
                                  {0xAC, 0xB7, 0x6D, 0x19, 0xD6, 0x44, 0x28,
                                   0xCD}},  // Toast Activator CLSID.
        .elevator_clsid = {0x708860E0,
                           0xF641,
                           0x4611,
                           {0x88, 0x95, 0x7D, 0x86, 0x7D, 0xD3, 0x67,
                            0x5B}},  // Elevator CLSID.
        .elevator_iid = {0x463abecf,
                         0x410d,
                         0x407f,
                         {0x8a, 0xf5, 0xd, 0xf3, 0x5a, 0x0, 0x5c,
                          0xc8}},  // IElevator IID and TypeLib
                                   // {463ABECF-410D-407F-8AF5-0DF35A005CC8}.
        .tracing_service_clsid = {0x70457024,
                                  0xd309,
                                  0x462c,
                                  {0xb2, 0xe0, 0x49, 0xa7, 0x71, 0x48, 0x7e,
                                   0x46}},  // SystemTraceSession CLSID.
        .tracing_service_iid = {0x056b3371,
                                0x1c09,
                                0x475b,
                                {0xa8, 0xd7, 0x9e, 0x58, 0xbf, 0x45, 0x53,
                                 0x3e}},  // ISystemTraceSessionChromium IID and
                                          // TypeLib
        .default_channel_name = L"",      // The empty string means "stable".
        .channel_strategy = ChannelStrategy::FLOATING,
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
            L"924012149-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome Beta
    {
        .size = sizeof(kInstallModes[0]),
        .index = BETA_INDEX,  // The mode for the side-by-side beta channel.
        .install_switch = "chrome-beta",  // Install switch.
        .install_suffix = L" Beta",       // Install suffix.
        .logo_suffix = L"Beta",           // Logo suffix.
        .app_guid =
            L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // A distinct app GUID.
        .base_app_name = L"Google Chrome Beta",    // A distinct base_app_name.
        .base_app_id = L"ChromeBeta",              // A distinct base_app_id.
        .browser_prog_id_prefix = L"ChromeBHTML",  // Browser ProgID prefix.
        .browser_prog_id_description =
            L"Chrome Beta HTML Document",     // Browser ProgID description.
        .pdf_prog_id_prefix = L"ChromeBPDF",  // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Chrome Beta PDF Document",  // PDF ProgID description.
        .active_setup_guid =
            L"{8237E44A-0054-442C-B6B6-EA0509993955}",  // Active Setup GUID.
        .legacy_command_execute_clsid = L"",  // CommandExecuteImpl CLSID.
        .toast_activator_clsid = {0xB89B137F,
                                  0x96AA,
                                  0x4AE2,
                                  {0x98, 0xC4, 0x63, 0x73, 0xEA, 0xA1, 0xEA,
                                   0x4D}},  // Toast Activator CLSID.
        .elevator_clsid = {0xDD2646BA,
                           0x3707,
                           0x4BF8,
                           {0xB9, 0xA7, 0x3, 0x86, 0x91, 0xA6, 0x8F,
                            0xC2}},  // Elevator CLSID.
        .elevator_iid = {0xa2721d66,
                         0x376e,
                         0x4d2f,
                         {0x9f, 0xf, 0x90, 0x70, 0xe9, 0xa4, 0x2b,
                          0x5f}},  // IElevator IID and TypeLib
                                   // {A2721D66-376E-4D2F-9F0F-9070E9A42B5F}.
        .tracing_service_clsid = {0xe64e8e46,
                                  0x6756,
                                  0x467e,
                                  {0x9f, 0x61, 0x86, 0x16, 0xac, 0x13, 0xd1,
                                   0xef}},  // SystemTraceSession CLSID.
        .tracing_service_iid = {0xa69d7d7d,
                                0x9a08,
                                0x422a,
                                {0xb6, 0xc6, 0xb7, 0xb8, 0xd3, 0x76, 0xa1,
                                 0x2c}},  // ISystemTraceSessionChromium IID and
                                          // TypeLib
        .default_channel_name = L"beta",  // Forced channel name.
        .channel_strategy = ChannelStrategy::FIXED,
        .supports_system_level = true,  // Supports system-level installs.
        .supports_set_as_default_browser =
            true,  // Supports in-product set as default browser UX.
        .app_icon_resource_index =
            icon_resources::kBetaApplicationIndex,  // App icon resource index.
        .app_icon_resource_id = IDR_X005_BETA,      // App icon resource id.
        // Using Beta icon instead of html/pdf-specific icons. The PDF bar would
        // interfere with the 'B' on the icon.
        .html_doc_icon_resource_index =
            icon_resources::kBetaApplicationIndex,  // HTML doc icon resource
                                                    // index.
        .pdf_doc_icon_resource_index =
            icon_resources::kBetaApplicationIndex,  // PDF doc icon resource
                                                    // index.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"924012151-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome Dev
    {
        .size = sizeof(kInstallModes[0]),
        .index = DEV_INDEX,  // The mode for the side-by-side dev channel.
        .install_switch = "chrome-dev",  // Install switch.
        .install_suffix = L" Dev",       // Install suffix.
        .logo_suffix = L"Dev",           // Logo suffix.
        .app_guid =
            L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // A distinct app GUID.
        .base_app_name = L"Google Chrome Dev",     // A distinct base_app_name.
        .base_app_id = L"ChromeDev",               // A distinct base_app_id.
        .browser_prog_id_prefix = L"ChromeDHTML",  // ProgID prefix.
        .browser_prog_id_description =
            L"Chrome Dev HTML Document",      // Browser ProgID description.
        .pdf_prog_id_prefix = L"ChromeDPDF",  // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Chrome Dev PDF Document",  // PDF ProgID description.
        .active_setup_guid =
            L"{401C381F-E0DE-4B85-8BD8-3F3F14FBDA57}",  // Active Setup GUID.
        .legacy_command_execute_clsid = L"",  // CommandExecuteImpl CLSID.
        .toast_activator_clsid = {0xF01C03EB,
                                  0xD431,
                                  0x4C83,
                                  {0x8D, 0x7A, 0x90, 0x27, 0x71, 0xE7, 0x32,
                                   0xFA}},  // Toast Activator CLSID.
        .elevator_clsid = {0xDA7FDCA5,
                           0x2CAA,
                           0x4637,
                           {0xAA, 0x17, 0x7, 0x40, 0x58, 0x4D, 0xE7,
                            0xDA}},  // Elevator CLSID.
        .elevator_iid = {0xbb2aa26b,
                         0x343a,
                         0x4072,
                         {0x8b, 0x6f, 0x80, 0x55, 0x7b, 0x8c, 0xe5,
                          0x71}},  // IElevator IID and TypeLib
                                   // {BB2AA26B-343A-4072-8B6F-80557B8CE571}.
        .tracing_service_clsid = {0x48c5c553,
                                  0x20f9,
                                  0x4cdc,
                                  {0x83, 0x40, 0x85, 0x29, 0xab, 0x83, 0xc5,
                                   0x52}},  // SystemTraceSession CLSID.
        .tracing_service_iid = {0xe08adae8,
                                0x9334,
                                0x46ed,
                                {0xb0, 0xcf, 0xdd, 0x17, 0x80, 0x15, 0x8d,
                                 0x55}},  // ISystemTraceSessionChromium IID and
                                          // TypeLib
        .default_channel_name = L"dev",   // Forced channel name.
        .channel_strategy = ChannelStrategy::FIXED,
        .supports_system_level = true,  // Supports system-level installs.
        .supports_set_as_default_browser =
            true,  // Supports in-product set as default browser UX.
        .app_icon_resource_index =
            icon_resources::kDevApplicationIndex,  // App icon resource index.
        .app_icon_resource_id = IDR_X004_DEV,      // App icon resource id.
        // Using Dev icon instead of html/pdf-specific icons. The PDF bar would
        // interfere with the 'D' on the icon.
        .html_doc_icon_resource_index =
            icon_resources::kDevApplicationIndex,  // HTML doc icon resource
                                                   // index.
        .pdf_doc_icon_resource_index =
            icon_resources::kDevApplicationIndex,  // PDF doc icon resource
                                                   // index.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"924012152-",  // App container sid prefix for sandbox.
    },
    // A secondary install mode for Google Chrome SxS (canary).
    {
        .size = sizeof(kInstallModes[0]),
        .index = CANARY_INDEX,  // The mode for the side-by-side canary channel.
        .install_switch = "chrome-sxs",  // Install switch.
        .install_suffix = L" SxS",       // Install suffix.
        .logo_suffix = L"Canary",        // Logo suffix.
        .app_guid =
            L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // A distinct app GUID.
        .base_app_name = L"Google Chrome Canary",  // A distinct base_app_name.
        .base_app_id = L"ChromeCanary",            // A distinct base_app_id.
        .browser_prog_id_prefix = L"ChromeSSHTM",  // ProgID prefix.
        .browser_prog_id_description =
            L"Chrome Canary HTML Document",    // Browser ProgID description.
        .pdf_prog_id_prefix = L"ChromeSSPDF",  // PDF ProgID prefix.
        .pdf_prog_id_description =
            L"Chrome Canary PDF Document",  // PDF ProgID description.
        .active_setup_guid =
            L"{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}",  // Active Setup GUID.
        .legacy_command_execute_clsid =
            L"{1BEAC3E3-B852-44F4-B468-8906C062422E}",  // CommandExecuteImpl
                                                        // CLSID.
        .toast_activator_clsid = {0xFA372A6E,
                                  0x149F,
                                  0x4E95,
                                  {0x83, 0x2D, 0x8F, 0x69, 0x8D, 0x40, 0xAD,
                                   0x7F}},  // Toast Activator CLSID.
        .elevator_clsid = {0x704C2872,
                           0x2049,
                           0x435E,
                           {0xA4, 0x69, 0xA, 0x53, 0x43, 0x13, 0xC4,
                            0x2B}},  // Elevator CLSID.
        .elevator_iid = {0x4f7ce041,
                         0x28e9,
                         0x484f,
                         {0x9d, 0xd0, 0x61, 0xa8, 0xca, 0xce, 0xfe,
                          0xe4}},  // IElevator IID and TypeLib
                                   // {4F7CE041-28E9-484F-9DD0-61A8CACEFEE4}.
        .tracing_service_clsid = {0x48c5c553,
                                  0x20f9,
                                  0x4cdc,
                                  {0x83, 0x40, 0x85, 0x29, 0xab, 0x83, 0xc5,
                                   0x52}},  // SystemTraceSession CLSID.
        .tracing_service_iid = {0x6efb8558,
                                0x68d1,
                                0x4826,
                                {0xa6, 0x12, 0xa1, 0x80, 0xb3, 0x57, 0x03,
                                 0x75}},  // ISystemTraceSessionChromium IID and
                                          // TypeLib
        .default_channel_name = L"canary",  // Forced channel name.
        .channel_strategy = ChannelStrategy::FIXED,
        .supports_system_level =
            false,  // Does not support system-level installs.
        .supports_set_as_default_browser =
            false,  // Does not support in-product set as default browser UX.
        .app_icon_resource_index =
            icon_resources::kSxSApplicationIndex,  // App icon resource index.
        .app_icon_resource_id = IDR_SXS,           // App icon resource id.
        // Using Canary icon instead of html/pdf-specific icons.
        .html_doc_icon_resource_index =
            icon_resources::kSxSApplicationIndex,  // HTML doc icon resource
                                                   // index.
        .pdf_doc_icon_resource_index =
            icon_resources::kSxSApplicationIndex,  // PDF doc icon resource
                                                   // index.
        .sandbox_sid_prefix =
            L"S-1-15-2-3251537155-1984446955-2931258699-841473695-1938553385-"
            L"924012150-",  // App container sid prefix for sandbox.
    },
};

static_assert(_countof(kInstallModes) == NUM_INSTALL_MODES,
              "Imbalance between kInstallModes and InstallConstantIndex");

}  // namespace install_static
