// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webnn/webnn_introspection_manager.h"

#include <utility>

#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom.h"

namespace webnn {

// static
WebNNIntrospectionManager* WebNNIntrospectionManager::GetInstance() {
  return base::Singleton<WebNNIntrospectionManager>::get();
}

WebNNIntrospectionManager::WebNNIntrospectionManager() = default;

WebNNIntrospectionManager::~WebNNIntrospectionManager() = default;

void WebNNIntrospectionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebNNIntrospectionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // If there is no opened webnn-internals page to receive the recorded data,
  // disable graph recording to save resources.
  if (observers_.empty()) {
    SetMLGraphRecordEnabled(false);
  }
#endif
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNIntrospectionManager::SetMLGraphRecordEnabled(bool enabled) {
  webnn_graph_record_enabled_ = enabled;

  for (auto& observer : observers_) {
    observer.OnGraphRecordEnabledChanged(webnn_graph_record_enabled_);
  }

  introspection_remotes_.Clear();
  receivers_.Clear();

  if (!webnn_graph_record_enabled_) {
    // Clearing `receivers_` will disable graph recording in existing
    // renderers.
    return;
  }

  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    content::RenderProcessHost* host = it.GetCurrentValue();
    if (host->IsInitializedAndNotDead()) {
      ConfigWebNNIntrospectionForProcess(host);
    }
  }
}

bool WebNNIntrospectionManager::IsMLGraphRecordEnabled() const {
  return webnn_graph_record_enabled_;
}

void WebNNIntrospectionManager::OnGraphRecorded(
    ::mojo_base::BigBuffer json_data) {
  for (auto& observer : observers_) {
    observer.OnGraphRecorded(json_data);
  }
}
#endif

void WebNNIntrospectionManager::OnRenderProcessHostCreated(
    content::RenderProcessHost* host) {
#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  // For new created RenderProcessHost, we only need to configure WebNN
  // introspection if graph recording is enabled because the default state is
  // disabled.
  ConfigWebNNIntrospectionForProcess(host);

#else
  // Avoid unused parameter warning when graph dump is disabled.
  std::ignore = host;
#endif
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
void WebNNIntrospectionManager::ConfigWebNNIntrospectionForProcess(
    content::RenderProcessHost* host) {
  if (!webnn_graph_record_enabled_) {
    return;
  }

  mojo::Remote<blink::mojom::WebNNIntrospection> debugger;
  host->BindReceiver(debugger.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::WebNNIntrospectionClient> client;
  receivers_.Add(this, client.InitWithNewPipeAndPassReceiver());
  debugger->SetClient(std::move(client));

  introspection_remotes_.Add(std::move(debugger));
}
#endif

}  // namespace webnn
