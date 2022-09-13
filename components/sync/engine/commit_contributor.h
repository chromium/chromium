// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTOR_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTOR_H_

#include <cstddef>
#include <memory>

namespace syncer {

class CommitContribution;

// This class represents a source of items to commit to the sync server.
//
// When asked, it can return CommitContribution objects that contain a set of
// items to be committed from this source.
class CommitContributor {
 public:
  CommitContributor() = default;
  virtual ~CommitContributor() = default;

  // Gathers up to |max_entries| unsynced items from this contributor into a
  // CommitContribution.  Returns null when the contributor has nothing to
  // contribute.
  virtual std::unique_ptr<CommitContribution> GetContribution(
      size_t max_entries) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_CONTRIBUTOR_H_
