// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/subresource_filter/content/common/ruleset_dealer.h"

namespace base {
class File;
}  // namespace base

namespace subresource_filter {

class MemoryMappedRuleset;

// The integrity verification status of a given ruleset version.
//
// A ruleset file starts from the kNotVerified state, after which it can be
// classified as kIntact, kCorrupt, or kInvalidFile upon integrity verification.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RulesetVerificationStatus {
  kNotVerified = 0,
  kIntact = 1,
  kCorrupt = 2,
  kInvalidFile = 3,
  kMaxValue = kInvalidFile,
};

// This class is the same as RulesetDealer, but additionally does a one-time
// integrity checking on the ruleset before handing it out from GetRuleset().
//
// The |status| of verification is persisted throughout the entire lifetime of
// |this| object, and is reset to kNotVerified only when a new ruleset is
// supplied to SetRulesetFile() method.
class VerifiedRulesetDealer : public RulesetDealer {
 public:
  class Handle;

  VerifiedRulesetDealer();
  ~VerifiedRulesetDealer() override;

  // RulesetDealer:
  void SetRulesetFile(base::File ruleset_file) override;
  scoped_refptr<const MemoryMappedRuleset> GetRuleset() override;

  // Opens file and use it as ruleset file on success. Returns valid
  // |base::File| in the case of file opened and set. Returns invalid
  // |base::File| in the case of file open error. In the case of error
  // ruleset dealer continues to use the previous file (if any).
  base::File OpenAndSetRulesetFile(int expected_checksum,
                                   const base::FilePath& file_path);

  // For tests only.
  RulesetVerificationStatus status() const { return status_; }

 private:
  RulesetVerificationStatus status_ = RulesetVerificationStatus::kNotVerified;
  // Associated with the current |ruleset_file_|;
  int expected_checksum_ = 0;

  DISALLOW_COPY_AND_ASSIGN(VerifiedRulesetDealer);
};

// The UI-thread handle that owns a VerifiedRulesetDealer living on a dedicated
// sequenced |task_runner|. Provides asynchronous access to the instance, and
// destroys it asynchronously.
class VerifiedRulesetDealer::Handle {
 public:
  // Creates a VerifiedRulesetDealer that is owned by this handle, accessed
  // through this handle, but lives on |task_runner|.
  explicit Handle(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~Handle();

  // Returns the |task_runner| on which the VerifiedRulesetDealer, as well as
  // the MemoryMappedRulesets it creates, should be accessed.
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Invokes |callback| on |task_runner|, providing a pointer to the underlying
  // VerifiedRulesetDealer as an argument. The pointer is guaranteed to be valid
  // at least until the callback returns.
  void GetDealerAsync(
      base::OnceCallback<void(VerifiedRulesetDealer*)> callback);

  // Schedules file open to use as a new ruleset file. In the case of success,
  // the new and valid |base::File| is passed to |callback|. In the case of
  // error an invalid |base::File| is passed to |callback| and dealer continues
  // to use previous ruleset file (if any).
  void TryOpenAndSetRulesetFile(const base::FilePath& path,
                                int expected_checksum,
                                base::OnceCallback<void(base::File)> callback);

 private:
  // Note: Raw pointer, |dealer_| already holds a reference to |task_runner_|.
  base::SequencedTaskRunner* task_runner_;
  std::unique_ptr<VerifiedRulesetDealer, base::OnTaskRunnerDeleter> dealer_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(Handle);
};

// Holds a strong reference to MemoryMappedRuleset, and provides acceess to it.
//
// Initially holds an empty reference, allowing this object to be created on the
// UI-thread synchronously, hence letting the Handle to be created synchronously
// and be immediately used by clients on the UI-thread, while the
// MemoryMappedRuleset is retrieved on the |task_runner| in a deferred manner.
class VerifiedRuleset {
 public:
  class Handle;

  VerifiedRuleset();
  ~VerifiedRuleset();

  // Can return nullptr even after initialization in case no ruleset is
  // available, or if the ruleset is corrupted.
  const MemoryMappedRuleset* Get() const {
    DCHECK(sequence_checker_.CalledOnValidSequence());
    return ruleset_.get();
  }

 private:
  // Initializes |ruleset_| with the one provided by the ruleset |dealer|.
  void Initialize(VerifiedRulesetDealer* dealer);

  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(VerifiedRuleset);
};

// The UI-thread handle that owns a VerifiedRuleset living on a dedicated
// sequenced |task_runner|, same as the VerifiedRulesetDealer lives on. Provides
// asynchronous access to the instance, and destroys it asynchronously.
class VerifiedRuleset::Handle {
 public:
  // Creates a VerifiedRuleset and initializes it asynchronously on a
  // |task_runner| using |dealer_handle|. The instance remains owned by this
  // handle, but living and accessed on the |task_runner|.
  explicit Handle(VerifiedRulesetDealer::Handle* dealer_handle);
  ~Handle();

  // Returns the |task_runner| on which the VerifiedRuleset, as well as the
  // MemoryMappedRuleset it holds a reference to, should be accessed.
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Invokes |callback| on |task_runner|, providing a pointer to the underlying
  // VerifiedRuleset as an argument. The pointer is guaranteed to be valid at
  // least until the callback returns.
  void GetRulesetAsync(base::OnceCallback<void(VerifiedRuleset*)> callback);

 private:
  // This is to allow ADSF to post |ruleset_.get()| pointer to |task_runner_|.
  friend class AsyncDocumentSubresourceFilter;

  // Note: Raw pointer, |ruleset_| already holds a reference to |task_runner_|.
  base::SequencedTaskRunner* task_runner_;
  std::unique_ptr<VerifiedRuleset, base::OnTaskRunnerDeleter> ruleset_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(Handle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_
