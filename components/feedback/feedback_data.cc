// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/feedback/feedback_util.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/tracing_manager.h"

namespace feedback {
namespace {

const char kTraceFilename[] = "tracing.zip";
const char kPerformanceCategoryTag[] = "Performance";

const base::FilePath::CharType kAutofillMetadataFilename[] =
    FILE_PATH_LITERAL("autofill_metadata.txt");
const char kAutofillMetadataAttachmentName[] = "autofill_metadata.zip";

const base::FilePath::CharType kHistogramsFilename[] =
    FILE_PATH_LITERAL("histograms.txt");
const char kHistogramsAttachmentName[] = "histograms.zip";

}  // namespace

FeedbackData::FeedbackData(base::WeakPtr<feedback::FeedbackUploader> uploader,
                           TracingManager* tracing_manager)
    : uploader_(std::move(uploader)) {
  // If tracing is enabled, the tracing manager should have been created before
  // sending the report. If it is created after this point, then the tracing is
  // not relevant to this report.
  if (tracing_manager) {
    tracing_manager_ = tracing_manager->AsWeakPtr();
  }
}

FeedbackData::~FeedbackData() = default;

void FeedbackData::OnFeedbackPageDataComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pending_op_count_--;
  SendReport();
}

void FeedbackData::CompressSystemInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (trace_id_ != 0) {
    ++pending_op_count_;
    if (!tracing_manager_ ||
        !tracing_manager_->GetTraceData(
            trace_id_,
            base::BindOnce(&FeedbackData::OnGetTraceData, this, trace_id_))) {
      pending_op_count_--;
      trace_id_ = 0;
    }
  }

  ++pending_op_count_;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FeedbackData::CompressLogs, this),
      base::BindOnce(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::SetAndCompressHistograms(std::string histograms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++pending_op_count_;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FeedbackData::CompressFile, this,
                     base::FilePath(kHistogramsFilename),
                     kHistogramsAttachmentName, std::move(histograms)),
      base::BindOnce(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::CompressAutofillMetadata() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string& autofill_info = autofill_metadata();
  if (autofill_info.empty()) {
    return;
  }
  // If the user opts out of sharing the page URL, any URL related entries
  // should be removed from the autofill logs.
  if (page_url().empty()) {
    feedback_util::RemoveUrlsFromAutofillData(autofill_info);
  }

  ++pending_op_count_;
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FeedbackData::CompressFile, this,
                     base::FilePath(kAutofillMetadataFilename),
                     kAutofillMetadataAttachmentName, std::move(autofill_info)),
      base::BindOnce(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::AttachAndCompressFileData(std::string attached_filedata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (attached_filedata.empty())
    return;
  ++pending_op_count_;
  base::FilePath attached_file =
      base::FilePath::FromUTF8Unsafe(attached_filename_);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&FeedbackData::CompressFile, this, attached_file,
                     std::string(), std::move(attached_filedata)),
      base::BindOnce(&FeedbackData::OnCompressComplete, this));
}

void FeedbackData::OnGetTraceData(
    int trace_id,
    scoped_refptr<base::RefCountedString> trace_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tracing_manager_)
    tracing_manager_->DiscardTraceData(trace_id);

  std::string s;
  std::swap(s, trace_data->as_string());
  AddFile(kTraceFilename, std::move(s));

  set_category_tag(kPerformanceCategoryTag);
  --pending_op_count_;
  trace_id_ = 0;
  SendReport();
}

void FeedbackData::OnCompressComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  --pending_op_count_;
  SendReport();
}

bool FeedbackData::IsDataComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_op_count_ == 0;
}

void FeedbackData::SendReport() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (uploader_ && IsDataComplete() && !report_sent_) {
    report_sent_ = true;
    userfeedback::ExtensionSubmit feedback_data;
    PrepareReport(&feedback_data);
    auto post_body = std::make_unique<std::string>();
    feedback_data.SerializeToString(post_body.get());
    uploader_->QueueReport(std::move(post_body),
                           /*has_email=*/!user_email().empty(),
                           /*product_id=*/feedback_data.product_id());
  }
}

}  // namespace feedback
