// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_CLIENT_CONNECTION_MANAGER_H_
#define COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_CLIENT_CONNECTION_MANAGER_H_

#include <unordered_set>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_creation_observer.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace heap_profiling {

class Controller;
enum class Mode;

// This class is responsible for connecting HeapProfilingClients to the
// HeapProfilingService.
//   * It registers itself as a content::NotificationObserver to listen for the
//     creation of the renderer processes.
//   * It registers itself as a content::BrowserChildProcessObserver to listen
//     for the creation of non-renderer processes.
// When a new process is created, it checks the current |Mode| to see whether
// the process should be profiled. If so, it grabs the HeapProfilingClient from
// the newly created process and connects it to the HeapProfilingService.
//
// This class is intended to be used from the browser/privileged process of the
// embedder.
//
// This class must be constructed/accessed/destroyed from the UI thread.
//
// This class can be subclassed for exactly one reason: to allow embedders to
// override AllowedToProfileRenderer in order to prevent incognito renderers
// from being profiled.
class ClientConnectionManager
    : public content::BrowserChildProcessObserver,
      public content::RenderProcessHostCreationObserver,
      content::NotificationObserver {
 public:
  // The owner of this instance must guarantee that |controller_| outlives this
  // class.
  // |controller| must be bound to the IO thread.
  ClientConnectionManager(base::WeakPtr<Controller> controller, Mode mode);

  ClientConnectionManager(const ClientConnectionManager&) = delete;
  ClientConnectionManager& operator=(const ClientConnectionManager&) = delete;

  ~ClientConnectionManager() override;

  // Start must be called immediately after the constructor. The only reason
  // that this is not a part of the constructor is to allow tests to skip this
  // step.
  void Start();

  Mode GetMode();

  // In addition to profiling `pid`, this will change the Mode to kManual. From
  // here on out, the caller must manually specify processes to be profiled.
  // Invokes `started_profiling_closure` if and when profiling starts
  // successfully.
  void StartProfilingProcess(base::ProcessId pid,
                             base::OnceClosure started_profiling_closure);

  virtual bool AllowedToProfileRenderer(content::RenderProcessHost* host);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeClientConnectionManager,
                           ShouldProfileNewRenderer);

  // Exists for testing only.
  void SetModeForTesting(Mode mode);

  // New processes will be profiled as they are created. Existing processes msut
  // be manually checked upon creation.
  void StartProfilingExistingProcessesIfNecessary();

  // BrowserChildProcessObserver
  // Observe connection of non-renderer child processes.
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data) override;

  void StartProfilingNonRendererChild(
      const content::ChildProcessData& data,
      base::OnceClosure started_profiling_closure = base::DoNothing());

  // content::RenderProcessHostCreationObserver
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  // NotificationObserver
  // Observe connection of renderer child processes.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  bool ShouldProfileNewRenderer(content::RenderProcessHost* renderer);

  void StartProfilingRenderer(
      content::RenderProcessHost* renderer,
      base::OnceClosure started_profiling_closure = base::DoNothing());

  // The owner of this instance must guarantee that |controller_| outlives this
  // class.
  // |controller_| must be bound to the IO thread.
  base::WeakPtr<Controller> controller_;

  Mode mode_;
  content::NotificationRegistrar registrar_;

  // This is used to identify the currently profiled renderers. Elements should
  // only be accessed on the UI thread and their values should be considered
  // opaque.
  //
  // Semantically, the elements must be something that identifies which specific
  // RenderProcess is being profiled. When the underlying RenderProcess goes
  // away, the element must be removed. The RenderProcessHost pointer and the
  // RenderProcessHostCreationObserver notification can be used to provide these
  // semantics.
  //
  // This variable represents renderers that have been instructed to start
  // profiling - it does not reflect whether a renderer is currently still being
  // profiled. That information is only known by the profiling service, and for
  // simplicity, it's easier to just track this variable in this process.
  std::unordered_set<void*> profiled_renderers_;
};

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_MULTI_PROCESS_CLIENT_CONNECTION_MANAGER_H_
