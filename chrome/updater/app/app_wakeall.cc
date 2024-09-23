// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_wakeall.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/util.h"

namespace updater {

// AppWakeAll finds and launches --wake applications for all versions of the
// updater within the same scope.
class AppWakeAll : public App {
 public:
  AppWakeAll() = default;

 private:
  ~AppWakeAll() override = default;

  // Overrides for App.
  void FirstTaskRun() override;
};

void AppWakeAll::FirstTaskRun() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](UpdaterScope scope) {
            std::optional<base::FilePath> base = GetInstallDirectory(scope);
            if (!base) {
              return kErrorNoBaseDirectory;
            }
            base::FileEnumerator(*base, false,
                                 base::FileEnumerator::DIRECTORIES)
                .ForEach([&scope](const base::FilePath& name) {
                  if (!base::Version(name.BaseName().MaybeAsASCII())
                           .IsValid()) {
                    return;
                  }
                  base::CommandLine command(
                      name.Append(GetExecutableRelativePath()));
                  command.AppendSwitch(kWakeSwitch);
                  if (IsSystemInstall(scope)) {
                    command.AppendSwitch(kSystemSwitch);
                  }
                  VLOG(1) << "Launching `" << command.GetCommandLineString()
                          << "`";
                  int exit = 0;
                  const base::Process process =
                      base::LaunchProcess(command, {});
                  if (!process.IsValid()) {
                    VPLOG(1) << "`" << command.GetCommandLineString()
                             << "` process invalid";
                  } else if (process.WaitForExitWithTimeout(base::Minutes(10),
                                                            &exit)) {
                    VLOG(1) << "`" << command.GetCommandLineString()
                            << "` exited " << exit;
                  } else {
                    VPLOG(1) << "`" << command.GetCommandLineString()
                             << "` timed out.";
                  }
                });
            return kErrorOk;
          },
          updater_scope()),
      base::BindOnce(&AppWakeAll::Shutdown, this));
}

scoped_refptr<App> MakeAppWakeAll() {
  return base::MakeRefCounted<AppWakeAll>();
}

}  // namespace updater
