// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_load_script_injector/renderer/on_load_script_injector.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace on_load_script_injector {

OnLoadScriptInjector::OnLoadScriptInjector(content::RenderFrame* frame)
    : RenderFrameObserver(frame), weak_ptr_factory_(this) {
  render_frame()
      ->GetAssociatedInterfaceRegistry()
      ->AddInterface<mojom::OnLoadScriptInjector>(
          base::BindRepeating(&OnLoadScriptInjector::BindToReceiver,
                              weak_ptr_factory_.GetWeakPtr()));
}

OnLoadScriptInjector::~OnLoadScriptInjector() = default;

void OnLoadScriptInjector::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::OnLoadScriptInjector> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OnLoadScriptInjector::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Don't inject anything for subframes.
  if (!render_frame()->IsMainFrame())
    return;

  for (base::ReadOnlySharedMemoryRegion& script : on_load_scripts_) {
    // Crude check to see this is UTF-16.
    DCHECK_EQ(script.GetSize() % sizeof(char16_t), 0u);

    auto mapping = script.Map();
    std::u16string script_converted(mapping.GetMemoryAs<char16_t>(),
                                    script.GetSize() / sizeof(char16_t));
    render_frame()->ExecuteJavaScript(script_converted);
  }
}

void OnLoadScriptInjector::AddOnLoadScript(
    base::ReadOnlySharedMemoryRegion script) {
  on_load_scripts_.push_back(std::move(script));
}

void OnLoadScriptInjector::ClearOnLoadScripts() {
  on_load_scripts_.clear();
}

void OnLoadScriptInjector::OnDestruct() {
  delete this;
}

}  // namespace on_load_script_injector
