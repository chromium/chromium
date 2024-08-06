// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"

#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace subresource_filter {

// VerifiedRulesetDealer and its Handle. ---------------------------------------

VerifiedRulesetDealer::VerifiedRulesetDealer(const RulesetConfig& config)
    : config_(config) {}
VerifiedRulesetDealer::~VerifiedRulesetDealer() = default;

RulesetFilePtr VerifiedRulesetDealer::OpenAndSetRulesetFile(
    int expected_checksum,
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // On Windows, open the file with FLAG_WIN_SHARE_DELETE to allow deletion
  // while there are handles to it still open.
  RulesetFilePtr file(
      new base::File(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                    base::File::FLAG_WIN_SHARE_DELETE),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "VerifiedRulesetDealer::OpenAndSetRulesetFile", "file_valid",
               file->IsValid());
  if (file->IsValid()) {
    SetRulesetFile(file->Duplicate());
    expected_checksum_ = expected_checksum;
  }
  return file;
}

void VerifiedRulesetDealer::SetRulesetFile(base::File ruleset_file) {
  RulesetDealer::SetRulesetFile(std::move(ruleset_file));
  status_ = RulesetVerificationStatus::kNotVerified;
  expected_checksum_ = 0;
}

scoped_refptr<const MemoryMappedRuleset> VerifiedRulesetDealer::GetRuleset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "VerifiedRulesetDealer::GetRuleset");

  switch (status_) {
    case RulesetVerificationStatus::kNotVerified: {
      auto ruleset = RulesetDealer::GetRuleset();
      if (ruleset) {
        if (IndexedRulesetMatcher::Verify(ruleset->data(), expected_checksum_,
                                          config_.uma_tag)) {
          status_ = RulesetVerificationStatus::kIntact;
        } else {
          status_ = RulesetVerificationStatus::kCorrupt;
          ruleset.reset();
        }
      } else {
        status_ = RulesetVerificationStatus::kInvalidFile;
      }
      return ruleset;
    }
    case RulesetVerificationStatus::kIntact: {
      auto ruleset = RulesetDealer::GetRuleset();
      // Note, |ruleset| can still be nullptr here if mmap fails, despite the
      // fact that it must have succeeded previously.
      return ruleset;
    }
    case RulesetVerificationStatus::kCorrupt:
    case RulesetVerificationStatus::kInvalidFile:
      return nullptr;
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

VerifiedRulesetDealer::Handle::Handle(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const RulesetConfig& config)
    : task_runner_(task_runner.get()),
      dealer_(new VerifiedRulesetDealer(config),
              base::OnTaskRunnerDeleter(std::move(task_runner))) {}

VerifiedRulesetDealer::Handle::~Handle() {
  // The `base::SequencedTaskRunner` that `task_runner_` points to is owned by
  // `dealer_`. Make sure to clear it before `dealer_` is destroyed to avoid
  // holding a dangling pointer.
  task_runner_ = nullptr;
}

void VerifiedRulesetDealer::Handle::GetDealerAsync(
    base::OnceCallback<void(VerifiedRulesetDealer*)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // NOTE: Properties of the sequenced |task_runner| guarantee that the
  // |callback| will always be provided with a valid pointer, because the
  // corresponding task will be posted *before* a task to delete the pointer
  // upon destruction of |this| Handler.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), dealer_.get()));
}

void VerifiedRulesetDealer::Handle::TryOpenAndSetRulesetFile(
    const base::FilePath& path,
    int expected_checksum,
    base::OnceCallback<void(RulesetFilePtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // |base::Unretained| is safe here because the |OpenAndSetRulesetFile| task
  // will be posted before a task to delete the pointer upon destruction of
  // |this| Handler.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&VerifiedRulesetDealer::OpenAndSetRulesetFile,
                     base::Unretained(dealer_.get()), expected_checksum, path),
      std::move(callback));
}

// VerifiedRuleset and its Handle. ---------------------------------------------

VerifiedRuleset::VerifiedRuleset() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VerifiedRuleset::~VerifiedRuleset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VerifiedRuleset::Initialize(VerifiedRulesetDealer* dealer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dealer, base::NotFatalUntil::M129);
  ruleset_ = dealer->GetRuleset();
}

VerifiedRuleset::Handle::Handle(VerifiedRulesetDealer::Handle* dealer_handle)
    : task_runner_(dealer_handle->task_runner()),
      ruleset_(new VerifiedRuleset,
               base::OnTaskRunnerDeleter(task_runner_.get())) {
  dealer_handle->GetDealerAsync(base::BindOnce(
      &VerifiedRuleset::Initialize, base::Unretained(ruleset_.get())));
}

VerifiedRuleset::Handle::~Handle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VerifiedRuleset::Handle::GetRulesetAsync(
    base::OnceCallback<void(VerifiedRuleset*)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), ruleset_.get()));
}

}  // namespace subresource_filter
