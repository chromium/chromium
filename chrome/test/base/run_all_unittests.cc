// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_io_thread.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "content/public/test/unittest_test_suite.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

#if defined(OS_WIN)
#include "chrome/install_static/test/scoped_install_details.h"
#endif

int main(int argc, char **argv) {
  base::PlatformThread::SetName("MainThread");

  // unit_tests don't currently work with the Network Service enabled.
  // https://crbug.com/966633.
  content::UnitTestTestSuite test_suite(new ChromeUnitTestSuite(argc, argv));

  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);

#if defined(OS_WIN)
  install_static::ScopedInstallDetails scoped_install_details;
#endif

  return base::LaunchUnitTests(
      argc, argv, base::Bind(&content::UnitTestTestSuite::Run,
                             base::Unretained(&test_suite)));
}
