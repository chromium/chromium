// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_PUBLISHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_PUBLISHER_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/subresource_filter/core/browser/ruleset_version.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/ruleset_config.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"

namespace subresource_filter {

class RulesetService;

// Owned by the underlying RulesetService. Its main responsibility is receiving
// new versions of subresource filtering rules from the RulesetService, and
// distributing them to renderer processes, where they will be memory-mapped
// as-needed by an UnverifiedRulesetDealer.
//
// Implementers must define `SendRulesetToRenderProcess()` as well as a mojo
// interface and implementation class that lives on the renderer to receive the
// ruleset.
class RulesetPublisher : public content::RenderProcessHostCreationObserver {
 public:
  RulesetPublisher(const RulesetPublisher&) = delete;
  RulesetPublisher& operator=(const RulesetPublisher&) = delete;

  ~RulesetPublisher() override;

  class Factory {
   public:
    virtual std::unique_ptr<RulesetPublisher> Create(
        RulesetService* ruleset_service,
        scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
        const = 0;
  };

  // Schedules file open and use it as ruleset file. In the case of success,
  // the new and valid |base::File| is passed to |callback|. In the case of
  // error an invalid |base::File| is passed to |callback|. The previous
  // ruleset file will be used (if any). In either case, the supplied
  // unique_ptr always contains a non-null |base::File|.
  virtual void TryOpenAndSetRulesetFile(
      const base::FilePath& file_path,
      int expected_checksum,
      base::OnceCallback<void(RulesetFilePtr)> callback);

  // Redistributes the new version of the |ruleset| to all existing consumers,
  // and sets up |ruleset| to be distributed to all future consumers.
  virtual void PublishNewRulesetVersion(RulesetFilePtr ruleset_data);

  // Task queue for best effort tasks in the thread the object was created in.
  // Used for tasks triggered on RulesetService instantiation so it doesn't
  // interfere with startup.  Runs in the UI thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> BestEffortTaskRunner();

  virtual VerifiedRulesetDealer::Handle* GetRulesetDealer();

  virtual void SetRulesetPublishedCallbackForTesting(
      base::OnceClosure callback);

  // Forwards calls to the underlying ruleset_service_.
  void IndexAndStoreAndPublishRulesetIfNeeded(
      const UnindexedRulesetInfo& unindex_ruleset_info);

 protected:
  // Protected to force instantiation through a `RulesetPublisher::Factory`.
  RulesetPublisher(
      RulesetService* ruleset_service,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const RulesetConfig& ruleset_config);

  virtual void SendRulesetToRenderProcess(base::File* file,
                                          content::RenderProcessHost* rph) = 0;

 private:
  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* rph) override;

  // The service owns the publisher, and therefore outlives it.
  raw_ptr<RulesetService> ruleset_service_ = nullptr;

  RulesetFilePtr ruleset_data_{nullptr, base::OnTaskRunnerDeleter{nullptr}};
  base::OnceClosure ruleset_published_callback_;

  std::unique_ptr<VerifiedRulesetDealer::Handle> ruleset_dealer_;
  scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_BROWSER_RULESET_PUBLISHER_H_
