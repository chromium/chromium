// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/child/pdf_child_init.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/win/current_module.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
typedef decltype(::GetFontData)* GetFontDataPtr;
GetFontDataPtr g_original_get_font_data = nullptr;


DWORD WINAPI GetFontDataPatch(HDC hdc,
                              DWORD table,
                              DWORD offset,
                              LPVOID buffer,
                              DWORD length) {
  DWORD rv = g_original_get_font_data(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      if (content::ChildThread::Get())
        content::ChildThread::Get()->PreCacheFont(logfont);
      rv = g_original_get_font_data(hdc, table, offset, buffer, length);
      if (content::ChildThread::Get())
        content::ChildThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void MaybePatchGdiGetFontData() {
#if BUILDFLAG(IS_WIN)
  // Only patch utility processes which explicitly need GDI.
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  auto service_sandbox_type =
      sandbox::policy::SandboxTypeFromCommandLine(command_line);
  bool need_gdi =
      service_sandbox_type == sandbox::mojom::Sandbox::kPrintCompositor ||
      service_sandbox_type == sandbox::mojom::Sandbox::kPdfConversion;
  if (!need_gdi)
    return;

#if defined(COMPONENT_BUILD)
  HMODULE module = ::GetModuleHandleA("pdfium.dll");
  DCHECK(module);
#else
  HMODULE module = CURRENT_MODULE();
#endif  // defined(COMPONENT_BUILD)

  // Need to patch GetFontData() for font loading to work correctly.
  // TODO(crbug.com/pdfium/11): Can be removed once PDFium switches to use Skia.
  static base::NoDestructor<base::win::IATPatchFunction> patch_get_font_data;
  patch_get_font_data->PatchFromModule(
      module, "gdi32.dll", "GetFontData",
      reinterpret_cast<void*>(GetFontDataPatch));
  g_original_get_font_data = reinterpret_cast<GetFontDataPtr>(
      patch_get_font_data->original_function());
#endif  // BUILDFLAG(IS_WIN)
}
