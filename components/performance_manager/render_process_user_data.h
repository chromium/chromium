// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/render_process_host_observer.h"

namespace content {

class RenderProcessHost;

}  // namespace content

namespace performance_manager {

class ProcessNodeImpl;

// Attached to RenderProcessHost as user data, associates the RenderProcessHost
// with the Resource Coordinator process node.
class RenderProcessUserData : public base::SupportsUserData::Data,
                              public content::RenderProcessHostObserver {
 public:
  // Observer interface to be notified when a RenderProcessUserData is
  // destroyed.
  class DestructionObserver {
   public:
    virtual ~DestructionObserver() = default;
    virtual void OnRenderProcessUserDataDestroying(
        content::RenderProcessHost*) = 0;
  };

  RenderProcessUserData(const RenderProcessUserData&) = delete;
  RenderProcessUserData& operator=(const RenderProcessUserData&) = delete;

  ~RenderProcessUserData() override;

  static const void* UserDataKey();

  static RenderProcessUserData* GetForRenderProcessHost(
      content::RenderProcessHost* host);

  // Registers an observer that is notified when the RenderProcessUserData is
  // destroyed. Can only be set to non-nullptr if it was previously nullptr, and
  // vice-versa.
  void SetDestructionObserver(DestructionObserver* destruction_observer);

  // Invoked when a process is launched for this RenderProcessHost
  // (immediately after RenderProcessHost::GetProcess() becomes valid).
  void OnProcessLaunched();

  ProcessNodeImpl* process_node() { return process_node_.get(); }

 private:
  friend class PerformanceManagerRegistryImpl;

  explicit RenderProcessUserData(
      content::RenderProcessHost* render_process_host);

  // Only PerformanceManagerRegistry is allowed to create a
  // RenderProcessUserData.
  static RenderProcessUserData* CreateForRenderProcessHost(
      content::RenderProcessHost* host);

  // RenderProcessHostObserver overrides
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  const raw_ptr<content::RenderProcessHost> host_;

  std::unique_ptr<ProcessNodeImpl> process_node_;

  raw_ptr<DestructionObserver> destruction_observer_ = nullptr;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
