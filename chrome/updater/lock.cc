// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/lock.h"

#include <memory>

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"
#include "components/named_system_lock/lock.h"

#if BUILDFLAG(IS_POSIX)
#include <string>

#include "base/strings/strcat.h"
#include "chrome/updater/updater_branding.h"
#elif BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/util/win_util.h"
#endif

namespace updater {

std::unique_ptr<ScopedLock> CreateScopedLock(const std::string& name,
                                             UpdaterScope scope,
                                             base::TimeDelta timeout) {
#if BUILDFLAG(IS_LINUX)
  return named_system_lock::ScopedLock::Create(
      base::StrCat({"/" PRODUCT_FULLNAME_STRING, name,
                    UpdaterScopeToString(scope), ".lock"}),
      timeout);
#elif BUILDFLAG(IS_MAC)
  return named_system_lock::ScopedLock::Create(
      base::StrCat({MAC_BUNDLE_IDENTIFIER_STRING, name,
                    UpdaterScopeToString(scope), ".lock"}),
      timeout);
#elif BUILDFLAG(IS_WIN)
  NamedObjectAttributes lock_attr =
      GetNamedObjectAttributes(base::ASCIIToWide(name).c_str(), scope);
  return named_system_lock::ScopedLock::Create(lock_attr.name, &lock_attr.sa,
                                               timeout);
#endif
}

}  // namespace updater
