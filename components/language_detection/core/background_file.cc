// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/background_file.h"

#include "base/functional/bind.h"

namespace language_detection {

BackgroundFile::TaskRunnerBoundFile::TaskRunnerBoundFile(
    base::File file,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : file_(std::move(file)), background_task_runner_(background_task_runner) {}

BackgroundFile::TaskRunnerBoundFile::~TaskRunnerBoundFile() {
  Invalidate();
}

void BackgroundFile::TaskRunnerBoundFile::Invalidate() {
  if (file_.IsValid()) {
    // This ensures that the file is closed on the background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce([](base::File) {}, std::move(file_)));
  }
}

BackgroundFile::TaskRunnerBoundFile::TaskRunnerBoundFile(
    TaskRunnerBoundFile&& other) = default;
BackgroundFile::TaskRunnerBoundFile&
BackgroundFile::TaskRunnerBoundFile::operator=(TaskRunnerBoundFile&& other) {
  Invalidate();
  file_ = std::move(other.file_);
  background_task_runner_ = std::move(other.background_task_runner_);
  return *this;
}

BackgroundFile::BackgroundFile(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : file_(base::File(), background_task_runner),
      background_task_runner_(background_task_runner) {}

BackgroundFile::~BackgroundFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BackgroundFile::InvalidateFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  file_ = TaskRunnerBoundFile(base::File(), background_task_runner_);
}

BackgroundFile::TaskRunnerBoundFile BackgroundFile::TaskRunnerBoundFile::Create(
    BackgroundFile::FileOpener file_opener,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  return BackgroundFile::TaskRunnerBoundFile(std::move(file_opener).Run(),
                                             background_task_runner);
}

void BackgroundFile::ReplaceFile(FileOpener file_opener,
                                 ReplacedCallback replaced_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Without `TaskRunnerBoundFile` the file returned by `file_opener` will
  // be destroyed on the wrong sequence if the weak pointer becomes null
  // before the reply occurs. See https://crbug.com/366698727.
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&BackgroundFile::TaskRunnerBoundFile::Create,
                     std::move(file_opener), background_task_runner_),
      base::BindOnce(&BackgroundFile::SwapFile, weak_ptr_factory_.GetWeakPtr(),
                     std::move(replaced_callback)));
}

void BackgroundFile::SwapFile(ReplacedCallback callback,
                              TaskRunnerBoundFile new_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The original `file_` will be destroyed at the end of this scope.
  std::swap(file_, new_file);
  std::move(callback).Run();
}
}  //  namespace language_detection
