// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_VERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_VERIFIED_RULESET_DEALER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/subresource_filter/core/common/ruleset_config.h"
#include "components/subresource_filter/core/common/ruleset_dealer.h"

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

// The unique_ptr ensures that the file is always closed on the proper task
// runner. See crbug.com/1182000
using RulesetFilePtr = std::unique_ptr<base::File, base::OnTaskRunnerDeleter>;

// This class is the same as RulesetDealer, but additionally does a one-time
// integrity checking on the ruleset before handing it out from GetRuleset().
//
// The |status| of verification is persisted throughout the entire lifetime of
// |this| object, and is reset to kNotVerified only when a new ruleset is
// supplied to SetRulesetFile() method.
class VerifiedRulesetDealer : public RulesetDealer {
 public:
  class Handle;

  explicit VerifiedRulesetDealer(const RulesetConfig& config);

  VerifiedRulesetDealer(const VerifiedRulesetDealer&) = delete;
  VerifiedRulesetDealer& operator=(const VerifiedRulesetDealer&) = delete;

  ~VerifiedRulesetDealer() override;

  // RulesetDealer:
  void SetRulesetFile(base::File ruleset_file) override;
  scoped_refptr<const MemoryMappedRuleset> GetRuleset() override;

  // Opens file and use it as ruleset file on success. Returns valid
  // |base::File| in the case of file opened and set. Returns invalid
  // |base::File| in the case of file open error. In the case of error
  // ruleset dealer continues to use the previous file (if any). In both
  // cases, the returned unique_ptr contains a non-null |base::File|.
  RulesetFilePtr OpenAndSetRulesetFile(int expected_checksum,
                                       const base::FilePath& file_path);

  // For tests only.
  RulesetVerificationStatus status() const { return status_; }

 private:
  RulesetVerificationStatus status_ = RulesetVerificationStatus::kNotVerified;
  // Associated with the current |ruleset_file_|;
  int expected_checksum_ = 0;
  RulesetConfig config_;
};

// The UI-thread handle that owns a VerifiedRulesetDealer living on a dedicated
// sequenced |task_runner|. Provides asynchronous access to the instance, and
// destroys it asynchronously.
class VerifiedRulesetDealer::Handle {
 public:
  // Creates a VerifiedRulesetDealer that is owned by this handle, accessed
  // through this handle, but lives on |task_runner|.
  explicit Handle(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  const RulesetConfig& config);

  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;

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
  // to use previous ruleset file (if any). In either case, the unique_ptr
  // supplied to the callback contains a non-null |base::File|.
  void TryOpenAndSetRulesetFile(
      const base::FilePath& path,
      int expected_checksum,
      base::OnceCallback<void(RulesetFilePtr)> callback);

 private:
  // Note: Raw pointer, |dealer_| already holds a reference to |task_runner_|.
  raw_ptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<VerifiedRulesetDealer, base::OnTaskRunnerDeleter> dealer_;
  SEQUENCE_CHECKER(sequence_checker_);
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

  VerifiedRuleset(const VerifiedRuleset&) = delete;
  VerifiedRuleset& operator=(const VerifiedRuleset&) = delete;

  ~VerifiedRuleset();

  // Can return nullptr even after initialization in case no ruleset is
  // available, or if the ruleset is corrupted.
  const MemoryMappedRuleset* Get() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ruleset_.get();
  }

 private:
  // Initializes |ruleset_| with the one provided by the ruleset |dealer|.
  void Initialize(VerifiedRulesetDealer* dealer);

  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  SEQUENCE_CHECKER(sequence_checker_);
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

  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;

  ~Handle();

  // Returns the |task_runner| on which the VerifiedRuleset, as well as the
  // MemoryMappedRuleset it holds a reference to, should be accessed.
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Invokes |callback| on |task_runner|, providing a pointer to the underlying
  // VerifiedRuleset as an argument. The pointer is guaranteed to be valid at
  // least until the callback returns.
  void GetRulesetAsync(base::OnceCallback<void(VerifiedRuleset*)> callback);

  base::WeakPtr<Handle> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  // This is to allow ADSF to post |ruleset_.get()| pointer to |task_runner_|.
  friend class AsyncDocumentSubresourceFilter;

  // Note: Raw pointer, |ruleset_| already holds a reference to |task_runner_|.
  raw_ptr<base::SequencedTaskRunner, DanglingUntriaged> task_runner_;
  std::unique_ptr<VerifiedRuleset, base::OnTaskRunnerDeleter> ruleset_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<Handle> weak_ptr_factory_{this};
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_VERIFIED_RULESET_DEALER_H_
