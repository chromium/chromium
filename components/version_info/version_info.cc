// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/version_info/version_info.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sanitizer_buildflags.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/version_info/version_info_values.h"

namespace version_info {

const std::string& GetProductNameAndVersionForUserAgent() {
  static const base::NoDestructor<std::string> product_and_version(
      "Chrome/" + GetVersionNumber());
  return *product_and_version;
}

const std::string GetProductNameAndVersionForReducedUserAgent(
    const std::string& build_version) {
  std::string product_and_version;
  base::StrAppend(&product_and_version, {"Chrome/", GetMajorVersionNumber(),
                                         ".0.", build_version, ".0"});
  return product_and_version;
}

std::string GetProductName() {
  return PRODUCT_NAME;
}

std::string GetVersionNumber() {
  return PRODUCT_VERSION;
}

int GetMajorVersionNumberAsInt() {
  DCHECK(GetVersion().IsValid());
  return GetVersion().components()[0];
}

std::string GetMajorVersionNumber() {
  return base::NumberToString(GetMajorVersionNumberAsInt());
}

const base::Version& GetVersion() {
  static const base::NoDestructor<base::Version> version(GetVersionNumber());
  return *version;
}

std::string GetLastChange() {
  return LAST_CHANGE;
}

bool IsOfficialBuild() {
  return IS_OFFICIAL_BUILD;
}

std::string GetOSType() {
#if BUILDFLAG(IS_WIN)
  return "Windows";
#elif BUILDFLAG(IS_IOS)
  return "iOS";
#elif BUILDFLAG(IS_MAC)
  return "Mac OS X";
#elif BUILDFLAG(IS_CHROMEOS)
# if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return "ChromeOS";
# else
  return "ChromiumOS";
# endif
#elif BUILDFLAG(IS_ANDROID)
  return "Android";
#elif BUILDFLAG(IS_LINUX)
  return "Linux";
#elif BUILDFLAG(IS_FREEBSD)
  return "FreeBSD";
#elif BUILDFLAG(IS_OPENBSD)
  return "OpenBSD";
#elif BUILDFLAG(IS_SOLARIS)
  return "Solaris";
#elif BUILDFLAG(IS_FUCHSIA)
  return "Fuchsia";
#else
  return "Unknown";
#endif
}

std::string GetChannelString(Channel channel) {
  switch (channel) {
    case Channel::STABLE:
      return "stable";
    case Channel::BETA:
      return "beta";
    case Channel::DEV:
      return "dev";
    case Channel::CANARY:
      return "canary";
    case Channel::UNKNOWN:
      return "unknown";
  }
  NOTREACHED();
  return std::string();
}

std::string GetSanitizerList() {
  std::string sanitizers;
#if defined(ADDRESS_SANITIZER)
  sanitizers += "address ";
#endif
#if BUILDFLAG(IS_HWASAN)
  sanitizers += "hwaddress ";
#endif
#if defined(LEAK_SANITIZER)
  sanitizers += "leak ";
#endif
#if defined(MEMORY_SANITIZER)
  sanitizers += "memory ";
#endif
#if defined(THREAD_SANITIZER)
  sanitizers += "thread ";
#endif
#if defined(UNDEFINED_SANITIZER)
  sanitizers += "undefined ";
#endif
  return sanitizers;
}

}  // namespace version_info
