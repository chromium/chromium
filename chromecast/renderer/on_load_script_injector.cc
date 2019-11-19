// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/on_load_script_injector.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/shared_memory_handle.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace chromecast {
namespace shell {

OnLoadScriptInjector::OnLoadScriptInjector(content::RenderFrame* frame)
    : RenderFrameObserver(frame), weak_ptr_factory_(this) {
  render_frame()->GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&OnLoadScriptInjector::BindToReceiver,
                          weak_ptr_factory_.GetWeakPtr()));
}

OnLoadScriptInjector::~OnLoadScriptInjector() {}

void OnLoadScriptInjector::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::OnLoadScriptInjector> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void OnLoadScriptInjector::DidCommitProvisionalLoad(
    bool is_same_document_navigation,
    ui::PageTransition transition) {
  // Ignore pushState or document fragment navigation.
  if (is_same_document_navigation)
    return;

  // Don't inject anything for subframes.
  if (!render_frame()->IsMainFrame())
    return;

  for (std::string& script : on_load_scripts_) {
    base::string16 script_utf16 = base::UTF8ToUTF16(script);
    render_frame()->ExecuteJavaScript(script_utf16);
  }
}

void OnLoadScriptInjector::AddOnLoadScript(const std::string& script) {
  on_load_scripts_.push_back(std::move(script));
}

void OnLoadScriptInjector::ClearOnLoadScripts() {
  on_load_scripts_.clear();
}

void OnLoadScriptInjector::OnDestruct() {
  delete this;
}

}  // namespace shell
}  // namespace chromecast
