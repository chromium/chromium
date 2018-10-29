// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_DELEGATE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_DELEGATE_H_

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"

namespace subresource_filter {

// Interface for a delegate that implements RulesetService operations that
// depend on content/, thus allowing the service to not directly depend on it.
class RulesetServiceDelegate {
 public:
  virtual ~RulesetServiceDelegate() = default;

  // Schedules file open and use it as ruleset file. In the case of success,
  // the new and valid |base::File| is passed to |callback|. In the case of
  // error an invalid |base::File| is passed to |callback|. The previous
  // ruleset file will be used (if any).
  virtual void TryOpenAndSetRulesetFile(
      const base::FilePath& file_path,
      int expected_checksum,
      base::OnceCallback<void(base::File)> callback) = 0;

  // Redistributes the new version of the |ruleset| to all existing consumers,
  // and sets up |ruleset| to be distributed to all future consumers.
  virtual void PublishNewRulesetVersion(base::File ruleset_data) = 0;

  // Task queue for best effort tasks in the thread the object was created in.
  // Used for tasks triggered on RulesetService instantiation so it doesn't
  // interfere with startup.  Runs in the UI thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  BestEffortTaskRunner() = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CORE_BROWSER_RULESET_SERVICE_DELEGATE_H_
