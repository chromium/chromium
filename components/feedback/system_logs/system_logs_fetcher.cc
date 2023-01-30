// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/system_logs/system_logs_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/feedback/feedback_report.h"
#endif

namespace system_logs {

namespace {

// List of keys in the SystemLogsResponse map whose corresponding values will
// not be redacted.
constexpr const char* const kExemptKeysOfUUIDs[] = {
    "CHROMEOS_BOARD_APPID",
    "CHROMEOS_CANARY_APPID",
    "CHROMEOS_RELEASE_APPID",
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kLacrosLogEntryPrefix[] = "Lacros ";
#endif

// Returns true if the given |key| and its corresponding value are exempt from
// redaction.
bool IsKeyExempt(const std::string& key) {
  for (auto* const exempt_key : kExemptKeysOfUUIDs) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::string MergeStingsByComma(const std::string& str1,
                               const std::string str2) {
  if (str1.empty())
    return str2;

  if (str2.empty())
    return str1;

  return str1 + ", " + str2;
}
#endif

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MergeAshAndLacrosCrashReportIdsInReponse();
#endif

  RunCallbackAndDeleteSoon();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// TODO(https://crbug.com/1156750): Add test cases to exercise this code path.
void SystemLogsFetcher::MergeAshAndLacrosCrashReportIdsInReponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Merge the lacros and ash recent crash report ids into a single log entry
  // with the key defined by kCrashReportIdsKey, i.e. crash_report_ids.
  auto ash_crash_iter =
      response_->find(feedback::FeedbackReport::kCrashReportIdsKey);
  // If ash crash_report_ids log entry is not found, it means CrashIdsSource
  // is not included in log sources, for example, the code is called by
  // BuildShellSystemLogsFetcher. Stop further processing.
  if (ash_crash_iter == response_->end())
    return;
  std::string ash_crash_report_ids = ash_crash_iter->second;

  std::string lacros_crash_report_ids;
  std::string lacros_crash_report_key =
      std::string(kLacrosLogEntryPrefix) +
      feedback::FeedbackReport::kCrashReportIdsKey;
  auto lacros_crash_iter = response_->find(lacros_crash_report_key);
  if (lacros_crash_iter != response_->end())
    lacros_crash_report_ids = lacros_crash_iter->second;

  // Update the crash_report_ids with the merged value.
  ash_crash_iter->second =
      MergeStingsByComma(ash_crash_report_ids, lacros_crash_report_ids);
  // Remove the lacros log entry of recent crash report ids.
  response_->erase(lacros_crash_report_key);

  // Merge the lacros and ash all crash report ids into a single log entry
  // with key defined by kAllCrashReportIdsKey, i.e. all_crash_report_ids.
  auto ash_all_crash_iter =
      response_->find(feedback::FeedbackReport::kAllCrashReportIdsKey);
  DCHECK(ash_all_crash_iter != response_->end());
  std::string ash_all_crash_report_ids = ash_all_crash_iter->second;

  std::string lacros_all_crash_report_ids;
  std::string lacros_all_crash_report_key =
      std::string(kLacrosLogEntryPrefix) +
      feedback::FeedbackReport::kAllCrashReportIdsKey;
  auto lacros_all_crash_iter = response_->find(lacros_all_crash_report_key);
  if (lacros_all_crash_iter != response_->end())
    lacros_all_crash_report_ids = lacros_all_crash_iter->second;

  std::string all_crash_report_ids;
  // If there are only recent crashes from Lacros, let lacros crash ids
  // go first; otherwise, ash crash ids will go first.
  if (ash_crash_report_ids.empty() && !lacros_crash_report_ids.empty()) {
    all_crash_report_ids = MergeStingsByComma(lacros_all_crash_report_ids,
                                              ash_all_crash_report_ids);
  } else {
    all_crash_report_ids = MergeStingsByComma(ash_all_crash_report_ids,
                                              lacros_all_crash_report_ids);
  }

  // Update all_crash_report_ids with merged value.
  ash_all_crash_iter->second = all_crash_report_ids;
  // Remove the Lacros log entry of all crash report ids.
  response_->erase(lacros_all_crash_report_key);
}
#endif

void SystemLogsFetcher::RunCallbackAndDeleteSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(std::move(response_));
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

}  // namespace system_logs
