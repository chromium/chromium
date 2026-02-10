// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_H_
#define CHROME_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace webnn {

// A singleton that manages WebNN introspection and debugging capabilities
// across all renderer processes.
//
// It observes RenderProcessHost creation to configure renderer-side
// introspection features, such as WebNN graph recording. As a central hub, it
// coordinates feature states and relays debugging data (like recorded graphs)
// from renderers to browser-side observers.
class WebNNIntrospectionManager
    : public blink::mojom::WebNNIntrospectionClient,
      public content::RenderProcessHostCreationObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
    virtual void OnGraphRecorded(const mojo_base::BigBuffer& json_data) = 0;
    virtual void OnGraphRecordEnabledChanged(bool is_enabled) {}
#endif
  };

  static WebNNIntrospectionManager* GetInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  void SetMLGraphRecordEnabled(bool enabled);
  bool IsMLGraphRecordEnabled() const;

  void OnGraphRecorded(::mojo_base::BigBuffer json_data) override;
#endif

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(content::RenderProcessHost* host) override;

 private:
  friend struct base::DefaultSingletonTraits<WebNNIntrospectionManager>;
  WebNNIntrospectionManager();
  ~WebNNIntrospectionManager() override;

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
  void ConfigWebNNIntrospectionForProcess(content::RenderProcessHost* host);

  bool webnn_graph_record_enabled_ = false;
#endif

  mojo::ReceiverSet<blink::mojom::WebNNIntrospectionClient> receivers_;
  base::ObserverList<Observer> observers_;
  mojo::RemoteSet<blink::mojom::WebNNIntrospection> introspection_remotes_;
};

}  // namespace webnn

#endif  // CHROME_BROWSER_WEBNN_WEBNN_INTROSPECTION_MANAGER_H_
