// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/simple_media_task_runner.h"

#include "base/task/single_thread_task_runner.h"

namespace chromecast {
namespace media {

SimpleMediaTaskRunner::SimpleMediaTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

SimpleMediaTaskRunner::~SimpleMediaTaskRunner() {
}

bool SimpleMediaTaskRunner::PostMediaTask(const base::Location& from_here,
                                          base::OnceClosure task,
                                          base::TimeDelta timestamp) {
  return task_runner_->PostTask(from_here, std::move(task));
}

}  // namespace media
}  // namespace chromecast
