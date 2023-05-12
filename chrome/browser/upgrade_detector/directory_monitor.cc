// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/directory_monitor.h"

#include <memory>
#include <utility>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/upgrade_detector/installed_version_monitor.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#endif

namespace {

base::FilePath GetDefaultMonitorLocation() {
#if BUILDFLAG(IS_MAC)
  return base::apple::OuterBundlePath();
#else
  return base::PathService::CheckedGet(base::DIR_EXE);
#endif
}

}  // namespace

DirectoryMonitor::DirectoryMonitor(base::FilePath install_dir)
    : install_dir_(std::move(install_dir)) {
  DCHECK(!install_dir_.empty());
}

DirectoryMonitor::~DirectoryMonitor() {
  if (watcher_)
    task_runner_->DeleteSoon(FROM_HERE, std::move(watcher_));
}

void DirectoryMonitor::Start(Callback on_change_callback) {
  DCHECK(!watcher_);
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()});
  watcher_ = std::make_unique<base::FilePathWatcher>();

#if BUILDFLAG(IS_MAC)
  // The normal Watch risks triggering a macOS Catalina+ consent dialog, so use
  // a trivial watch here.
  const base::FilePathWatcher::Type watch_type =
      base::FilePathWatcher::Type::kTrivial;
#else
  const base::FilePathWatcher::Type watch_type =
      base::FilePathWatcher::Type::kNonRecursive;
#endif

  // Start the watcher on a background sequence, reporting a failure to start to
  // |on_change_callback| on the caller's sequence. The watcher is given a
  // trampoline that will run |on_change_callback| on the caller's sequence.
  // base::Unretained is safe because the watcher instance lives on the target
  // sequence and will be destroyed there in a subsequent task.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &base::FilePathWatcher::Watch, base::Unretained(watcher_.get()),
          std::move(install_dir_), watch_type,
          base::BindRepeating(
              [](base::SequencedTaskRunner* main_sequence,
                 const Callback& on_change_callback, const base::FilePath&,
                 bool error) {
                main_sequence->PostTask(
                    FROM_HERE, base::BindOnce(on_change_callback, error));
              },
              base::RetainedRef(base::SequencedTaskRunner::GetCurrentDefault()),
              on_change_callback)),
      base::BindOnce(
          [](const Callback& on_change_callback, bool start_result) {
            if (!start_result)
              on_change_callback.Run(/*error=*/true);
          },
          on_change_callback));
}

// static
std::unique_ptr<InstalledVersionMonitor> InstalledVersionMonitor::Create() {
  return std::make_unique<DirectoryMonitor>(GetDefaultMonitorLocation());
}
