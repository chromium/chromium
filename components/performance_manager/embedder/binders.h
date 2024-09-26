// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions for binding renderer objects to their performance_manager
// Graph node counterparts in the browser process.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_

#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
}  // namespace content

namespace performance_manager {

// Binds Mojo interfaces used by PerformanceManager. Accessed through
// PerformanceManagerRegistry::GetBinders().
class Binders {
 public:
  Binders() = default;
  ~Binders() = default;

  Binders(const Binders&) = delete;
  Binders& operator=(const Binders&) = delete;

  // Allows the remote renderer process to bind to the corresponding ProcessNode
  // in the graph. Typically wired up via
  // ContentBrowserClient::ExposeInterfacesToRenderer().
  void ExposeInterfacesToRendererProcess(
      service_manager::BinderRegistry* registry,
      content::RenderProcessHost* host);

  // Allows the remote renderer frame to bind to its corresponding FrameNode in
  // the graph. Typically wired up via
  // ContentBrowserClient::RegisterBrowserInterfaceBindersForFrame().
  void ExposeInterfacesToRenderFrame(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_BINDERS_H_
