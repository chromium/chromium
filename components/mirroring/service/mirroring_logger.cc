// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_logger.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/mirroring/mojom/session_observer.mojom.h"

namespace mirroring {

constexpr char kLogPrefix[] = "mirroring.";

MirroringLogger::MirroringLogger(std::string_view prefix,
                                 mojo::Remote<mojom::SessionObserver>& observer)
    : prefix_(prefix),
      observer_(observer),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}
MirroringLogger::~MirroringLogger() = default;

void MirroringLogger::LogInfo(std::string_view message) {
  const std::string log_message =
      base::StrCat({kLogPrefix, prefix_, ": ", message});
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MirroringLogger::LogInfoInternal,
                                  weak_ptr_factory_.GetWeakPtr(), log_message));
  } else {
    LogInfoInternal(log_message);
  }
}

void MirroringLogger::LogError(std::string_view message) {
  const std::string log_message =
      base::StrCat({kLogPrefix, prefix_, ": ", message});
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MirroringLogger::LogErrorInternal,
                                  weak_ptr_factory_.GetWeakPtr(), log_message));
  } else {
    LogErrorInternal(log_message);
  }
}

void MirroringLogger::LogError(mojom::SessionError error,
                               std::string_view message) {
  const std::string log_message = base::StrCat(
      {kLogPrefix, prefix_, ": SessionError (",
       base::NumberToString(static_cast<int>(error)), "), ", message});
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MirroringLogger::LogErrorInternal,
                                  weak_ptr_factory_.GetWeakPtr(), log_message));
  } else {
    LogErrorInternal(log_message);
  }
}

void MirroringLogger::LogInfoInternal(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << message;
  if (observer_.get()) {
    observer_.get()->LogInfoMessage(message);
  }
}

void MirroringLogger::LogErrorInternal(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(ERROR) << message;
  if (observer_.get()) {
    observer_.get()->LogErrorMessage(message);
  }
}

}  // namespace mirroring
