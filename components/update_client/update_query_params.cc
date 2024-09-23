// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_query_params.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/update_client/update_query_params_delegate.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace update_client {

namespace {

const char kUnknown[] = "unknown";

// The request extra information is the OS and architecture, this helps
// the server select the right package to be delivered.
const char kOs[] =
#if BUILDFLAG(IS_APPLE)
    "mac";
#elif BUILDFLAG(IS_WIN)
    "win";
#elif BUILDFLAG(IS_ANDROID)
    "android";
#elif BUILDFLAG(IS_CHROMEOS)
    "cros";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    "linux";
#elif BUILDFLAG(IS_FUCHSIA)
    "fuchsia";
#elif BUILDFLAG(IS_OPENBSD)
    "openbsd";
#else
#error "unknown os"
#endif

const char kArch[] =
#if defined(ARCH_CPU_X86_64)
    "x64";
#elif defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_ARMEL)
    "arm";
#elif defined(ARCH_CPU_ARM64)
    "arm64";
#elif defined(ARCH_CPU_MIPS64EL)
    "mips64el";
#elif defined(ARCH_CPU_MIPSEL)
    "mipsel";
#elif defined(__powerpc64__)
    "ppc64";
#elif defined(ARCH_CPU_LOONGARCH32)
        "loongarch32";
#elif defined(ARCH_CPU_LOONGARCH64)
        "loongarch64";
#elif defined(ARCH_CPU_RISCV64)
        "riscv64";
#else
#error "unknown arch"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kChrome[] = "chrome";
const char kCrx[] = "chromecrx";
const char kWebView[] = "googleandroidwebview";
#else
const char kChrome[] = "chromium";
const char kCrx[] = "chromiumcrx";
const char kWebView[] = "androidwebview";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

UpdateQueryParamsDelegate* g_delegate = nullptr;

}  // namespace

// static
std::string UpdateQueryParams::Get(ProdId prod) {
  return base::StringPrintf(
      "os=%s&arch=%s&os_arch=%s&nacl_arch=%s&prod=%s%s&acceptformat=crx3,puff",
      kOs, kArch, base::SysInfo().OperatingSystemArchitecture().c_str(),
      GetNaclArch(), GetProdIdString(prod),
      g_delegate ? g_delegate->GetExtraParams().c_str() : "");
}

// static
const char* UpdateQueryParams::GetProdIdString(UpdateQueryParams::ProdId prod) {
  switch (prod) {
    case UpdateQueryParams::CHROME:
      return kChrome;
    case UpdateQueryParams::CRX:
      return kCrx;
    case UpdateQueryParams::WEBVIEW:
      return kWebView;
  }
  return kUnknown;
}

// static
const char* UpdateQueryParams::GetOS() {
  return kOs;
}

// static
const char* UpdateQueryParams::GetArch() {
  return kArch;
}

// static
const char* UpdateQueryParams::GetNaclArch() {
#if defined(ARCH_CPU_X86_FAMILY)
#if defined(ARCH_CPU_X86_64)
  return "x86-64";
#elif BUILDFLAG(IS_WIN)
  bool x86_64 = base::win::OSInfo::GetInstance()->IsWowX86OnAMD64();
  return x86_64 ? "x86-64" : "x86-32";
#else
  return "x86-32";
#endif
#elif defined(ARCH_CPU_ARM_FAMILY)
  return "arm";
#elif defined(ARCH_CPU_MIPSEL)
  return "mips32";
#elif defined(ARCH_CPU_MIPS64EL)
  return "mips64";
#elif defined(ARCH_CPU_PPC64)
  return "ppc64";
#elif defined(ARCH_CPU_LOONGARCH32)
  return "loongarch32";
#elif defined(ARCH_CPU_LOONGARCH64)
  return "loongarch64";
#elif defined(ARCH_CPU_RISCV64)
  return "riscv64";
#else
  // NOTE: when adding new values here, please remember to update the
  // comment in the .h file about possible return values from this function.
#error "You need to add support for your architecture here"
#endif
}

// static
std::string UpdateQueryParams::GetProdVersion() {
  return std::string(version_info::GetVersionNumber());
}

// static
void UpdateQueryParams::SetDelegate(UpdateQueryParamsDelegate* delegate) {
  CHECK(!g_delegate || !delegate || (delegate == g_delegate));
  g_delegate = delegate;
}

}  // namespace update_client
