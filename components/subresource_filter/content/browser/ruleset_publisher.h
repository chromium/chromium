// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"

namespace subresource_filter {

// Interface for a RulesetService that defines operations that distribute the
// ruleset to the renderer processes via the RulesetDealer.
class RulesetPublisher {
 public:
  virtual ~RulesetPublisher() = default;

  // Schedules file open and use it as ruleset file. In the case of success,
  // the new and valid |base::File| is passed to |callback|. In the case of
  // error an invalid |base::File| is passed to |callback|. The previous
  // ruleset file will be used (if any). In either case, the supplied
  // unique_ptr always contains a non-null |base::File|.
  virtual void TryOpenAndSetRulesetFile(
      const base::FilePath& file_path,
      int expected_checksum,
      base::OnceCallback<void(RulesetFilePtr)> callback) = 0;

  // Redistributes the new version of the |ruleset| to all existing consumers,
  // and sets up |ruleset| to be distributed to all future consumers.
  virtual void PublishNewRulesetVersion(RulesetFilePtr ruleset_data) = 0;

  // Task queue for best effort tasks in the thread the object was created in.
  // Used for tasks triggered on RulesetService instantiation so it doesn't
  // interfere with startup.  Runs in the UI thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  BestEffortTaskRunner() = 0;

  // Gets the ruleset dealer associated with the RulesetPublisher.
  virtual VerifiedRulesetDealer::Handle* GetRulesetDealer() = 0;

  // Set the callback on publish associated with the RulesetPublisher.
  virtual void SetRulesetPublishedCallbackForTesting(
      base::OnceClosure callback) = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_H_
