// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_switches.h"

namespace {

// On Linux, the Chrome binary may have been replaced by an update. Ask it to
// report its version.
InstalledAndCriticalVersion GetInstalledVersionSynchronous() {
  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitch(switches::kProductVersion);
  base::Version installed_version;
  std::string reply;
  if (base::GetAppOutput(command_line, &reply)) {
    installed_version =
        base::Version(base::TrimWhitespaceASCII(reply, base::TRIM_ALL));
  }
  // Failure may be a result of invoking a Chrome that predates the introduction
  // of the --product-version switch in https://crrev.com/48795 (6.0.424.0).
  DLOG_IF(ERROR, !installed_version.IsValid())
      << "Failed to get current file version; child process replied with: "
      << reply;

  return InstalledAndCriticalVersion(std::move(installed_version));
}

}  // namespace

void GetInstalledVersion(InstalledVersionCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&GetInstalledVersionSynchronous), std::move(callback));
}
