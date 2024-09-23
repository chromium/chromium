// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_SCOPED_CHANNEL_OVERRIDE_H_
#define CHROME_TEST_BASE_SCOPED_CHANNEL_OVERRIDE_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/test/scoped_install_details.h"
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include <optional>
#include <string>
#endif

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#error ScopedChannelOverride is only supported for Google Chrome builds.
#endif

namespace chrome {

// Allows a test to override the browser's channel for a limited time (e.g., for
// the duration of a test). This minimally impacts the behavior of the functions
// in chrome/common/channel_info.h. On Windows, the override is also reflected
// in the process-wide install_static::InstallDetails instance.
class ScopedChannelOverride {
 public:
  enum class Channel {
    kExtendedStable,
    kStable,
    kBeta,
    kDev,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    kCanary,
#endif
  };

  explicit ScopedChannelOverride(Channel channel);
  ~ScopedChannelOverride();

 private:
#if BUILDFLAG(IS_WIN)
  install_static::ScopedInstallDetails scoped_install_details_;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
  // The original value of the CHROME_VERSION_EXTRA environment variable.
  const std::optional<std::string> old_env_var_;
#endif
};

}  // namespace chrome

#endif  // CHROME_TEST_BASE_SCOPED_CHANNEL_OVERRIDE_H_
