// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_
#define COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_

#include <list>
#include <string>

#include "base/macros.h"
#include "base/time/time.h"

namespace feed {

class ContentOperation;

// Native counterpart of ContentMutation.java.
// To commit a set of ContentOperation into FeedContentDatabase, user need to
// create a ContentMutation, next use Append* methods to append operations
// into the mutation, and then pass the ContentMutation to
// |FeedContentDatabase::CommitContentMutation| to commit operations.
class ContentMutation {
 public:
  ContentMutation();
  ~ContentMutation();

  void AppendDeleteOperation(std::string key);
  void AppendDeleteAllOperation();
  void AppendDeleteByPrefixOperation(std::string prefix);
  void AppendUpsertOperation(std::string key, std::string value);

  // Check if mutation has ContentOperation left.
  bool Empty();

  // Return the number of operations in the mutation.
  size_t Size() const;

  base::TimeTicks GetStartTime() const;

  // This will remove the first ContentOperation in |operations_list_| and
  // return it to caller.
  ContentOperation TakeFirstOperation();

 private:
  std::list<ContentOperation> operations_list_;

  const base::TimeTicks start_time_;

  DISALLOW_COPY_AND_ASSIGN(ContentMutation);
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_FEED_CONTENT_MUTATION_H_
