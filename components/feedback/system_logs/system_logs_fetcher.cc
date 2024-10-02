// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/system_logs/system_logs_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace system_logs {

namespace {

// List of keys in the SystemLogsResponse map whose corresponding values will
// not be redacted.
constexpr const char* const kKeysExemptOfRedaction[] = {
    "CHROMEOS_BOARD_APPID",
    "CHROMEOS_CANARY_APPID",
    "CHROMEOS_RELEASE_APPID",
    // Base64-encoded binary data are exempted to keep them from getting
    // corrupted by individual redaction tools.
    "cros_ec_panicinfo",
    "i915_error_state",
    "perf-data",
    "perfetto-data",
    // Contains URL-like app-ids which should not be redacted.
    "app_service",
};

// Returns true if the given |key| and its corresponding value are exempt from
// redaction.
bool IsKeyExempt(const std::string& key) {
  for (auto* const exempt_key : kKeysExemptOfRedaction) {
    if (key == exempt_key)
      return true;
  }
  return false;
}

// Runs the Redaction tool over the entris of |response|.
void Redact(redaction::RedactionTool* redactor, SystemLogsResponse* response) {
  for (auto& element : *response) {
    if (!IsKeyExempt(element.first))
      element.second = redactor->Redact(element.second);
  }
}

}  // namespace

SystemLogsFetcher::SystemLogsFetcher(
    bool scrub_data,
    const char* const first_party_extension_ids[])
    : response_(std::make_unique<SystemLogsResponse>()),
      num_pending_requests_(0),
      task_runner_for_redactor_(
          scrub_data
              ? base::ThreadPool::CreateSequencedTaskRunner(
                    // User visible because this is called when the user is
                    // looking at the send feedback dialog, watching a spinner.
                    {base::TaskPriority::USER_VISIBLE,
                     base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
              : nullptr),
      redactor_(scrub_data ? std::make_unique<redaction::RedactionTool>(
                                 first_party_extension_ids)
                           : nullptr) {}

SystemLogsFetcher::~SystemLogsFetcher() {
  // Ensure that destruction happens on same sequence where the object is being
  // accessed.
  if (redactor_) {
    DCHECK(task_runner_for_redactor_);
    task_runner_for_redactor_->DeleteSoon(FROM_HERE, std::move(redactor_));
  }
}

void SystemLogsFetcher::AddSource(std::unique_ptr<SystemLogsSource> source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_sources_.emplace_back(std::move(source));
  num_pending_requests_++;
}

void SystemLogsFetcher::Fetch(SysLogsFetcherCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = std::move(callback);

  if (data_sources_.empty()) {
    RunCallbackAndDeleteSoon();
    return;
  }

  for (size_t i = 0; i < data_sources_.size(); ++i) {
    VLOG(1) << "Fetching SystemLogSource: " << data_sources_[i]->source_name();
    data_sources_[i]->Fetch(base::BindOnce(&SystemLogsFetcher::OnFetched,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           data_sources_[i]->source_name()));
  }
}

void SystemLogsFetcher::OnFetched(
    const std::string& source_name,
    std::unique_ptr<SystemLogsResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(response);

  VLOG(1) << "Received SystemLogSource: " << source_name;

  if (redactor_) {
    // It is safe to pass the unretained redactor_ instance here because
    // the redactor_ is owned by |this| and |this| only deletes itself
    // once all responses have been collected and added (see AddResponse()).
    SystemLogsResponse* response_ptr = response.get();
    task_runner_for_redactor_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(Redact, base::Unretained(redactor_.get()),
                       base::Unretained(response_ptr)),
        base::BindOnce(&SystemLogsFetcher::AddResponse,
                       weak_ptr_factory_.GetWeakPtr(), source_name,
                       std::move(response)));
  } else {
    AddResponse(source_name, std::move(response));
  }
}

void SystemLogsFetcher::AddResponse(
    const std::string& source_name,
    std::unique_ptr<SystemLogsResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& it : *response) {
    // An element with a duplicate key would not be successfully inserted.
    bool ok = response_->emplace(it).second;
    DCHECK(ok) << "Duplicate key found: " << it.first;
  }

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  RunCallbackAndDeleteSoon();
}

void SystemLogsFetcher::RunCallbackAndDeleteSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(response_));
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

}  // namespace system_logs
