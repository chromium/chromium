// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_query_params.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/update_client/update_query_params_delegate.h"
#include "components/version_info/version_info.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace update_client {

namespace {

const char kUnknown[] = "unknown";

// The request extra information is the OS and architecture, this helps
// the server select the right package to be delivered.
const char kOs[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#elif defined(OS_ANDROID)
    "android";
#elif defined(OS_CHROMEOS)
    "cros";
#elif defined(OS_LINUX)
    "linux";
#elif defined(OS_FUCHSIA)
    "fuchsia";
#elif defined(OS_OPENBSD)
    "openbsd";
#else
#error "unknown os"
#endif

const char kArch[] =
#if defined(__amd64__) || defined(_WIN64)
    "x64";
#elif defined(__i386__) || defined(_WIN32)
    "x86";
#elif defined(__arm__)
    "arm";
#elif defined(__aarch64__)
    "arm64";
#elif defined(__mips__) && (__mips == 64)
    "mips64el";
#elif defined(__mips__)
    "mipsel";
#elif defined(__powerpc64__)
    "ppc64";
#else
#error "unknown arch"
#endif

const char kChrome[] = "chrome";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kCrx[] = "chromecrx";
#else
const char kCrx[] = "chromiumcrx";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

UpdateQueryParamsDelegate* g_delegate = nullptr;

}  // namespace

// static
std::string UpdateQueryParams::Get(ProdId prod) {
  return base::StringPrintf(
      "os=%s&arch=%s&os_arch=%s&nacl_arch=%s&prod=%s%s&acceptformat=crx3", kOs,
      kArch, base::SysInfo().OperatingSystemArchitecture().c_str(),
      GetNaclArch(), GetProdIdString(prod),
      g_delegate ? g_delegate->GetExtraParams().c_str() : "");
}

// static
const char* UpdateQueryParams::GetProdIdString(UpdateQueryParams::ProdId prod) {
  switch (prod) {
    case UpdateQueryParams::CHROME:
      return kChrome;
      break;
    case UpdateQueryParams::CRX:
      return kCrx;
      break;
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
#elif defined(OS_WIN)
  bool x86_64 = (base::win::OSInfo::GetInstance()->wow64_status() ==
                 base::win::OSInfo::WOW64_ENABLED);
  return x86_64 ? "x86-64" : "x86-32";
#else
  return "x86-32";
#endif
#elif defined(ARCH_CPU_ARMEL)
  return "arm";
#elif defined(ARCH_CPU_ARM64)
  return "arm64";
#elif defined(ARCH_CPU_MIPSEL)
  return "mips32";
#elif defined(ARCH_CPU_MIPS64EL)
  return "mips64";
#elif defined(ARCH_CPU_PPC64)
  return "ppc64";
#else
// NOTE: when adding new values here, please remember to update the
// comment in the .h file about possible return values from this function.
#error "You need to add support for your architecture here"
#endif
}

// static
std::string UpdateQueryParams::GetProdVersion() {
  return version_info::GetVersionNumber();
}

// static
void UpdateQueryParams::SetDelegate(UpdateQueryParamsDelegate* delegate) {
  DCHECK(!g_delegate || !delegate || (delegate == g_delegate));
  g_delegate = delegate;
}

}  // namespace update_client
