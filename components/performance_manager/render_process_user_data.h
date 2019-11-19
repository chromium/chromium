// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_

#include <memory>

#include "base/macros.h"
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
  ~RenderProcessUserData() override;

  static RenderProcessUserData* GetForRenderProcessHost(
      content::RenderProcessHost* host);
  static RenderProcessUserData* GetOrCreateForRenderProcessHost(
      content::RenderProcessHost* host);

  // Detaches all instances from their RenderProcessHosts and destroys them.
  static void DetachAndDestroyAll();

  ProcessNodeImpl* process_node() { return process_node_.get(); }

 private:
  explicit RenderProcessUserData(
      content::RenderProcessHost* render_process_host);

  // RenderProcessHostObserver overrides
  void RenderProcessReady(content::RenderProcessHost* host) override;
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // All instances are linked together in a doubly linked list to allow orderly
  // destruction at browser shutdown time.
  static RenderProcessUserData* first_;

  RenderProcessUserData* prev_ = nullptr;
  RenderProcessUserData* next_ = nullptr;

  content::RenderProcessHost* const host_;

  std::unique_ptr<ProcessNodeImpl> process_node_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessUserData);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
