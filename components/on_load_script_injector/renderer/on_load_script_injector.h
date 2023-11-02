// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
#define COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/on_load_script_injector/export.h"
#include "components/on_load_script_injector/on_load_script_injector.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace on_load_script_injector {

// Injects one or more scripts into a RenderFrame at the earliest possible time
// during the page load process.
class ON_LOAD_SCRIPT_INJECTOR_EXPORT OnLoadScriptInjector
    : public content::RenderFrameObserver,
      public mojom::OnLoadScriptInjector {
 public:
  explicit OnLoadScriptInjector(content::RenderFrame* frame);

  OnLoadScriptInjector(const OnLoadScriptInjector&) = delete;
  OnLoadScriptInjector& operator=(const OnLoadScriptInjector&) = delete;

  void BindToReceiver(
      mojo::PendingAssociatedReceiver<mojom::OnLoadScriptInjector> receiver);

  // mojom::OnLoadScriptInjector implementation.
  void AddOnLoadScript(base::ReadOnlySharedMemoryRegion script) override;
  void ClearOnLoadScripts() override;

  // RenderFrameObserver overrides.
  void OnDestruct() override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;

 private:
  // Called by OnDestruct(), when the RenderFrame is destroyed.
  ~OnLoadScriptInjector() override;

  std::vector<base::ReadOnlySharedMemoryRegion> on_load_scripts_;
  mojo::AssociatedReceiverSet<mojom::OnLoadScriptInjector> receivers_;
  base::WeakPtrFactory<OnLoadScriptInjector> weak_ptr_factory_;
};

}  // namespace on_load_script_injector

#endif  // COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_RENDERER_ON_LOAD_SCRIPT_INJECTOR_H_
