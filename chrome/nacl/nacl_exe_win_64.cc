// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "chrome/app/chrome_crash_reporter_client_win.h"
#include "chrome/install_static/product_install_details.h"
#include "components/crash/content/app/breakpad_win.h"
#include "components/nacl/loader/nacl_helper_win_64.h"
#include "content/public/common/content_switches.h"

namespace {

base::LazyInstance<ChromeCrashReporterClient>::Leaky g_chrome_crash_client =
    LAZY_INSTANCE_INITIALIZER;

} // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
#if BUILDFLAG(IS_WIN)
  install_static::InitializeProductDetailsForPrimaryModule();
#endif

  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  crash_reporter::SetCrashReporterClient(g_chrome_crash_client.Pointer());
  breakpad::InitCrashReporter(process_type);

  return nacl::NaClWin64Main();
}
