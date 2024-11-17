// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_

#include <memory>
#include <vector>

#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class BrowserContext;
class RenderProcessHost;
class NavigationHandle;
class NavigationThrottle;
class WebContents;
}  // namespace content

namespace performance_manager {

class Binders;

// Allows tracking of WebContents, RenderProcessHosts and SharedWorkerInstances
// in the PerformanceManager.
//
// A process that embeds the PerformanceManager should create a single instance
// of this and notify it when WebContents, RenderProcessHosts or BrowserContexts
// are created.
//
// TearDown() must be called prior to destroying this object. This will schedule
// deletion of PageNodes, ProcessNodes and WorkerNodes retained by this
// registry, even if the associated WebContents, RenderProcessHosts and
// SharedWorkerInstances still exist.
//
// This class can only be accessed on the main thread.
class PerformanceManagerRegistry {
 public:
  using Throttles = std::vector<std::unique_ptr<content::NavigationThrottle>>;

  virtual ~PerformanceManagerRegistry() = default;

  PerformanceManagerRegistry(const PerformanceManagerRegistry&) = delete;
  void operator=(const PerformanceManagerRegistry&) = delete;

  // Creates a PerformanceManagerRegistry instance.
  static std::unique_ptr<PerformanceManagerRegistry> Create();

  // Returns the only instance of PerformanceManagerRegistry living in this
  // process, or nullptr if there is none.
  static PerformanceManagerRegistry* GetInstance();

  // Returns a helper that binds Mojo interfaces for PerformanceManager.
  virtual Binders& GetBinders() = 0;

  // Helper function that invokes CreatePageNodeForWebContents only if it hasn't
  // already been called for the provided WebContents.
  void MaybeCreatePageNodeForWebContents(content::WebContents* web_contents);

  // Must be invoked when a WebContents is created. Creates an associated
  // PageNode in the PerformanceManager, if it doesn't already exist. This
  // should only be called once for a given |web_contents|.
  virtual void CreatePageNodeForWebContents(
      content::WebContents* web_contents) = 0;

  // Sets the page type for a WebContents.
  virtual void SetPageType(content::WebContents* web_contents,
                           PageType type) = 0;

  // Must be invoked for a NavigationHandle when it is committed, allowing the
  // PM the opportunity to apply NavigationThrottles. Typically wired up to
  // ContentBrowserClient::CreateThrottlesForNavigation.
  virtual Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) = 0;

  // Must be invoked when a BrowserContext is added/removed.
  // Registers/unregisters an observer that creates WorkerNodes when
  // SharedWorkerInstances are added in the BrowserContext.
  virtual void NotifyBrowserContextAdded(
      content::BrowserContext* browser_context) = 0;
  virtual void NotifyBrowserContextRemoved(
      content::BrowserContext* browser_context) = 0;

  // Must be invoked when a renderer process is starting up to ensure that a
  // process node is created for the RPH. Typically wired up via
  // ContentBrowserClient::ExposeInterfacesToRenderer, which should also call
  // GetBinders().ExposeInterfacesToRendererProcess().
  // NOTE: Ideally we'd have a separate CreateProcessNode notification, but the
  // current content architecture makes it very difficult to get this
  // notification.
  virtual void CreateProcessNode(
      content::RenderProcessHost* render_process_host) = 0;

  // Must be invoked prior to destroying the object. Schedules deletion of
  // PageNodes and ProcessNodes retained by this registry, even if the
  // associated WebContents and RenderProcessHosts still exist.
  virtual void TearDown() = 0;

 protected:
  PerformanceManagerRegistry() = default;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_
