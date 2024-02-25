// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FILE_FLUSHER_H_
#define CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FILE_FLUSHER_H_

#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// Flushes files under the requested directories in the blocking pool. If the
// same directory is requested more than once, the last request cancels all
// previous ones and start a new flushing process.
class COMPONENT_EXPORT(ASH_BROWSER_CONTEXT_HELPER) FileFlusher {
 public:
  FileFlusher();

  FileFlusher(const FileFlusher&) = delete;
  FileFlusher& operator=(const FileFlusher&) = delete;

  ~FileFlusher();

  // Flush files under |path|.
  // |recursive| whether to go down and flush sub trees under |path|.
  // |callback| is invoked when the request is finished or canceled.
  void RequestFlush(const base::FilePath& path,
                    bool recursive,
                    base::OnceClosure callback);

  // Set a callback for every file flush for test. Note the callback is
  // called on a blocking pool thread.
  using OnFlushCallback = base::RepeatingCallback<void(const base::FilePath&)>;
  void set_on_flush_callback_for_test(
      const OnFlushCallback& on_flush_callback) {
    on_flush_callback_for_test_ = on_flush_callback;
  }

  void PauseForTest();
  void ResumeForTest();

 private:
  // Job for the flushing requests.
  class Job;

  // Starts the first job in |jobs_|.
  void ScheduleJob();

  // Invoked by a Job when it finishes.
  void OnJobDone(Job* job);

  // Not owned. Job manages its own life time.
  std::vector<raw_ptr<Job, VectorExperimental>> jobs_;

  // A callback for testing to be invoked when a file is flushed.
  OnFlushCallback on_flush_callback_for_test_;

  bool paused_for_test_ = false;

  base::WeakPtrFactory<FileFlusher> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BROWSER_CONTEXT_HELPER_FILE_FLUSHER_H_
