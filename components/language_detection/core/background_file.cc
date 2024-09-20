// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/background_file.h"

namespace language_detection {

BackgroundFile::BackgroundFile(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(background_task_runner) {}
BackgroundFile::~BackgroundFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateFile();
}

void BackgroundFile::InvalidateFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This ensures that the current file's destructor runs on a background thread
  // as that might involve blocking IO.
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce([](base::File) {}, std::move(file_)));
}

void BackgroundFile::ReplaceFile(FileOpener file_opener,
                                 ReplacedCallback replaced_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(file_opener),
      base::BindOnce(&BackgroundFile::SwapFile, weak_ptr_factory_.GetWeakPtr(),
                     std::move(replaced_callback)));
}

void BackgroundFile::SwapFile(ReplacedCallback callback, base::File new_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvalidateFile();
  file_ = std::move(new_file);
  std::move(callback).Run();
}
}  //  namespace language_detection
