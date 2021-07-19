// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/threading/platform_thread.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "content/public/test/unittest_test_suite.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) &&           \
    (defined(OS_WIN) || defined(OS_MAC) ||         \
     (defined(OS_POSIX) && !defined(OS_ANDROID) && \
      !BUILDFLAG(IS_CHROMEOS_ASH)))
#include "chrome/test/base/scoped_channel_override.h"
#elif defined(OS_WIN)
#include "chrome/install_static/test/scoped_install_details.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

int main(int argc, char** argv) {
  base::PlatformThread::SetName("MainThread");

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedDisableCrosapiForTesting disable_crosapi;
#endif

  // unit_tests don't currently work with the Network Service enabled.
  // https://crbug.com/966633.
  content::UnitTestTestSuite test_suite(new ChromeUnitTestSuite(argc, argv));

  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) &&           \
    (defined(OS_WIN) || defined(OS_MAC) ||         \
     (defined(OS_POSIX) && !defined(OS_ANDROID) && \
      !BUILDFLAG(IS_CHROMEOS_ASH)))
  // Tests running in Google Chrome builds on Win/Mac/Lin/Lacros should present
  // as stable channel by default.
  chrome::ScopedChannelOverride scoped_channel_override(
      chrome::ScopedChannelOverride::Channel::kStable);
#elif defined(OS_WIN)
  // Tests running in Chromium builds on Windows need basic InstallDetails even
  // though there are no channels.
  install_static::ScopedInstallDetails scoped_install_details;
#endif

  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
