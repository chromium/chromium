// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_report.h"

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/feedback/features.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_constants.h"
#include "components/feedback/proto/extension.pb.h"

namespace feedback {

namespace {

constexpr base::FilePath::CharType kFeedbackReportFilenameWildcard[] =
    FILE_PATH_LITERAL("Feedback Report.*");

constexpr char kFeedbackReportFilenamePrefix[] = "Feedback Report.";

void WriteReportOnBlockingPool(const base::FilePath reports_path,
                               const base::FilePath& file,
                               scoped_refptr<FeedbackReport> report) {
  DCHECK(reports_path.IsParent(file));
  if (!base::DirectoryExists(reports_path)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(reports_path, &error))
      return;
  }
  base::ImportantFileWriter::WriteFileAtomically(file, report->data(),
                                                 "FeedbackReport");
}

}  // namespace

FeedbackReport::FeedbackReport(
    const base::FilePath& path,
    const base::Time& upload_at,
    std::unique_ptr<std::string> data,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool has_email,
    int product_id)
    : has_email_(has_email),
      product_id_(product_id),
      reports_path_(path),
      upload_at_(upload_at),
      data_(std::move(data)),
      reports_task_runner_(task_runner) {
  if (reports_path_.empty())
    return;
  file_ = reports_path_.AppendASCII(
      kFeedbackReportFilenamePrefix +
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  reports_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteReportOnBlockingPool, reports_path_, file_,
                     base::WrapRefCounted<FeedbackReport>(this)));

  // Write feedback report to tmp directory when flag is on. This is for e2e
  // tast test verifying feedback report contains certain data. The tast test
  // will later clean it up after test is done.
  if (feedback::features::IsOsFeedbackSaveReportToLocalForE2ETestingEnabled()) {
    base::FilePath tmp_root;
    base::PathService::Get(base::DIR_TEMP, &tmp_root);
    const base::FilePath reports_path_for_tast =
        tmp_root.AppendASCII("feedback-report/");
    const base::FilePath file_for_tast =
        reports_path_for_tast.AppendASCII("feedback-report");

    reports_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WriteReportOnBlockingPool,
                                  reports_path_for_tast, file_for_tast,
                                  base::WrapRefCounted<FeedbackReport>(this)));
  }
}

FeedbackReport::FeedbackReport(
    base::FilePath path,
    std::unique_ptr<std::string> data,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    bool has_email,
    int product_id)
    : file_(path),
      has_email_(has_email),
      product_id_(product_id),
      data_(std::move(data)),
      reports_task_runner_(task_runner) {}

// static
const char FeedbackReport::kCrashReportIdsKey[]  = "crash_report_ids";

// static
const char FeedbackReport::kAllCrashReportIdsKey[] = "all_crash_report_ids";

// static
const char FeedbackReport::kMemUsageWithTabTitlesKey[] = "mem_usage_with_title";

// static
const char FeedbackReport::kFeedbackUserCtlConsentKey[] =
    "feedbackUserCtlConsent";

// static
void FeedbackReport::LoadReportsAndQueue(const base::FilePath& user_dir,
                                         const QueueCallback& callback) {
  if (user_dir.empty())
    return;

  base::FileEnumerator enumerator(user_dir,
                                  false,
                                  base::FileEnumerator::FILES,
                                  kFeedbackReportFilenameWildcard);
  for (base::FilePath name = enumerator.Next();
       !name.empty();
       name = enumerator.Next()) {
    auto data = std::make_unique<std::string>();
    if (ReadFileToString(name, data.get())) {
      userfeedback::ExtensionSubmit parsed;
      parsed.ParseFromString(*data);

      bool has_email = parsed.common_data().has_user_email() &&
                       !parsed.common_data().user_email().empty();
      callback.Run(base::MakeRefCounted<FeedbackReport>(
          std::move(name), std::move(data),
          base::SingleThreadTaskRunner::GetCurrentDefault(), has_email,
          parsed.product_id()));
    }
  }
}

void FeedbackReport::DeleteReportOnDisk() {
  reports_task_runner_->PostTask(FROM_HERE, base::GetDeleteFileCallback(file_));
}

bool FeedbackReport::should_include_variations() const {
  // TODO(b/307804234): Tie this to the report itself via ExtensionSubmit
  // instead of hardcoding the product IDs here.
  return product_id_ != feedback::kOrcaFeedbackProductId;
}

FeedbackReport::~FeedbackReport() = default;

}  // namespace feedback
