// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_REGISTRY_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_REGISTRY_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/browser_child_process_watcher.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/owned_objects.h"
#include "components/performance_manager/performance_manager_tab_helper.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "components/performance_manager/public/performance_manager_owned.h"
#include "components/performance_manager/public/performance_manager_registered.h"
#include "components/performance_manager/registered_objects.h"
#include "components/performance_manager/render_process_user_data.h"
#include "components/performance_manager/tab_helper_frame_node_source.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace performance_manager {

class PerformanceManagerMainThreadMechanism;
class PerformanceManagerMainThreadObserver;
class ServiceWorkerContextAdapterImpl;
class WorkerNodeImpl;
class WorkerWatcher;

class PerformanceManagerRegistryImpl
    : public content::RenderProcessHostCreationObserver,
      public PerformanceManagerRegistry,
      public PerformanceManagerTabHelper::DestructionObserver,
      public RenderProcessUserData::DestructionObserver {
 public:
  PerformanceManagerRegistryImpl();
  ~PerformanceManagerRegistryImpl() override;

  PerformanceManagerRegistryImpl(const PerformanceManagerRegistryImpl&) =
      delete;
  void operator=(const PerformanceManagerRegistryImpl&) = delete;

  // Returns the only instance of PerformanceManagerRegistryImpl living in this
  // process, or nullptr if there is none.
  static PerformanceManagerRegistryImpl* GetInstance();

  // Adds / removes an observer that is notified when a PageNode is created on
  // the main thread. Forwarded to from the public PerformanceManager interface.
  void AddObserver(PerformanceManagerMainThreadObserver* observer);
  void RemoveObserver(PerformanceManagerMainThreadObserver* observer);

  // Adds / removes main thread mechanisms. Forwarded to from the public
  // PerformanceManager interface.
  void AddMechanism(PerformanceManagerMainThreadMechanism* mechanism);
  void RemoveMechanism(PerformanceManagerMainThreadMechanism* mechanism);
  bool HasMechanism(PerformanceManagerMainThreadMechanism* mechanism);

  // PM owned objects. Forwarded to from the public PerformanceManager
  // interface. See performance_manager.h for details.
  void PassToPM(std::unique_ptr<PerformanceManagerOwned> pm_owned);
  std::unique_ptr<PerformanceManagerOwned> TakeFromPM(
      PerformanceManagerOwned* pm_owned);

  // PM registered objects. Forwarded to from the public PerformanceManager
  // interface. See performance_manager.h for details.
  void RegisterObject(PerformanceManagerRegistered* pm_object);
  void UnregisterObject(PerformanceManagerRegistered* object);
  PerformanceManagerRegistered* GetRegisteredObject(uintptr_t type_id);

  // PerformanceManagerRegistry:
  Binders& GetBinders() override;
  void CreatePageNodeForWebContents(
      content::WebContents* web_contents) override;
  void SetPageType(content::WebContents* web_contents, PageType type) override;
  Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) override;
  void NotifyBrowserContextAdded(
      content::BrowserContext* browser_context) override;
  void NotifyBrowserContextRemoved(
      content::BrowserContext* browser_context) override;
  void CreateProcessNode(
      content::RenderProcessHost* render_process_host) override;
  void TearDown() override;

  // PerformanceManagerTabHelper::DestructionObserver:
  void OnPerformanceManagerTabHelperDestroying(
      content::WebContents* web_contents) override;

  // RenderProcessUserData::DestructionObserver:
  void OnRenderProcessUserDataDestroying(
      content::RenderProcessHost* render_process_host) override;

  // This is exposed so that the tab helper can call it as well, as in some
  // testing configurations we otherwise miss RPH creation notifications that
  // usually arrive when interfaces are exposed to the renderer.
  void EnsureProcessNodeForRenderProcessHost(
      content::RenderProcessHost* render_process_host);

  // Gets the ProcessNode for the browser process from the
  // BrowserChildProcessWatcher.
  ProcessNodeImpl* GetBrowserProcessNode();

  // Gets the ProcessNode for the non-renderer child process with the given `id`
  // from the BrowserChildProcessWatcher.
  ProcessNodeImpl* GetBrowserChildProcessNode(BrowserChildProcessHostId id);

  // Searches all WorkerWatchers for a WorkerNode matching the given `token`.
  // Exposed so that accessors in performance_manager.h can look up WorkerNodes
  // on the UI thread.
  WorkerNodeImpl* FindWorkerNodeForToken(const blink::WorkerToken& token);

  size_t GetOwnedCountForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pm_owned_.size();
  }

  size_t GetRegisteredCountForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return pm_registered_.size();
  }

  // Returns the WorkerWatcher for `browser_context`, or nullptr if there is
  // none. Tests can call methods on the WorkerWatcher to simulate workers.
  WorkerWatcher* GetWorkerWatcherForTesting(
      content::BrowserContext* browser_context);

 private:
  friend class TestBrowserChildProcess;
  friend void DeleteBrowserProcessNodeForTesting();

  // Allow unit tests to register simulated child processes with
  // BrowserChildProcessWatcher.
  BrowserChildProcessWatcher& GetBrowserChildProcessWatcherForTesting();

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Helper object to bind Mojo interfaces. Can be accessed on any sequence.
  Binders binders_;

  // Tracks WebContents and RenderProcessHost for which we have created user
  // data. Used to destroy all user data when the registry is destroyed.
  base::flat_set<raw_ptr<content::WebContents, CtnExperimental>> web_contents_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<raw_ptr<content::RenderProcessHost, CtnExperimental>>
      render_process_hosts_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps each browser context to its ServiceWorkerContextAdapterImpl.
  base::flat_map<content::BrowserContext*,
                 std::unique_ptr<ServiceWorkerContextAdapterImpl>>
      service_worker_context_adapters_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Maps each browser context to its worker watcher.
  base::flat_map<content::BrowserContext*, std::unique_ptr<WorkerWatcher>>
      worker_watchers_ GUARDED_BY_CONTEXT(sequence_checker_);

  BrowserChildProcessWatcher browser_child_process_watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used by WorkerWatchers to access existing process nodes and frame
  // nodes.
  performance_manager::ProcessNodeSource process_node_source_
      GUARDED_BY_CONTEXT(sequence_checker_);
  performance_manager::TabHelperFrameNodeSource frame_node_source_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<PerformanceManagerMainThreadObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<PerformanceManagerMainThreadMechanism> mechanisms_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Objects owned by the PM.
  OwnedObjects<PerformanceManagerOwned,
               /* CallbackArgType = */ void,
               &PerformanceManagerOwned::OnPassedToPM,
               &PerformanceManagerOwned::OnTakenFromPM>
      pm_owned_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Storage for PerformanceManagerRegistered objects.
  RegisteredObjects<PerformanceManagerRegistered> pm_registered_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERFORMANCE_MANAGER_REGISTRY_IMPL_H_
