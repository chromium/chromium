// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_JOURNAL_MUTATION_H_
#define COMPONENTS_FEED_CORE_FEED_JOURNAL_MUTATION_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "components/feed/core/feed_journal_operation.h"

namespace feed {

// Native counterpart of JournalMutation.java.
// To commit a set of JournalOperation into FeedJournalDatabase, first,
// JournalMutation need to be created, then use Add* methods to add operations
// into the mutation, and pass the JournalMutation to
// FeedJournalDatabase::CommitJournalMutation to commit operations.
class JournalMutation {
 public:
  explicit JournalMutation(std::string journal_name);
  ~JournalMutation();

  void AddAppendOperation(std::string value);
  void AddCopyOperation(std::string to_journal_name);
  void AddDeleteOperation();

  const std::string& journal_name();

  // Check if mutation has JournalOperation left.
  bool Empty();

  // Return the number of operations in the mutation.
  size_t Size() const;

  base::TimeTicks GetStartTime() const;

  // This will remove the first JournalOperation in |operations_list_| and
  // return it to caller.
  JournalOperation TakeFirstOperation();

  JournalOperation::Type FirstOperationType();

 private:
  const std::string journal_name_;

  std::list<JournalOperation> operations_list_;

  const base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(JournalMutation);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_JOURNAL_MUTATION_H_
