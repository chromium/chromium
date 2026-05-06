// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/surface_embed_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
class WebContents;
}

namespace surface_embed {

class SurfaceEmbedHostCollection;

// The browser process counterpart to the SurfaceEmbedWebPlugin. This class
// bridges the plugin in the parent document and the SurfaceEmbedConnector owned
// by the child WebContents. It provides the plugin with the surface of the
// child WebContents and synchronizes visual properties with the connector.
// SurfaceEmbedHost is owned by the parent RenderFrameHost, but will also go
// away if the embedded WebContents is destroyed. It is available on
// the RenderFrameHost that hosts certain WebUI (currently, only
// WebUIBrowserUI). A RenderFrameHost can have multiple SurfaceEmbedHost, each
// of which corresponds to an
// <embed type="application/x-chromium-surface-embed"> element.
class SurfaceEmbedHost : public mojom::SurfaceEmbedHost,
                         public content::SurfaceEmbedConnector::Delegate {
 public:
  ~SurfaceEmbedHost() override;

  SurfaceEmbedHost(const SurfaceEmbedHost&) = delete;
  SurfaceEmbedHost& operator=(const SurfaceEmbedHost&) = delete;

  // Creates a new SurfaceEmbedHost. The host's lifetime is tied to the
  // RenderFrameHost and the mojo pipe, whichever is destroyed first.
  static SurfaceEmbedHost* Create(
      content::RenderFrameHost* rfh,
      mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver);

  // mojom::SurfaceEmbedHost implementation:
  void SetSurfaceEmbed(
      mojo::PendingRemote<mojom::SurfaceEmbed> surface_embed) override;
  void AttachConnector(const base::UnguessableToken& content_id) override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool is_visible) override;

  // content::SurfaceEmbedConnector::Delegate implementation:
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void UpdateLocalSurfaceIdFromChild(
      const viz::LocalSurfaceId& local_surface_id) override;
  void ChildProcessGone() override;
  void DetachedByHost() override;
  void RequestFocus() override;
  bool IsAttachedForTesting() const override;

  // TODO: Update surface_embed.mojom so that this is an override of a virtual
  // from mojom::SurfaceEmbedHost.
  void DetachConnector();

  void SetDestructionCallbackForTesting(base::OnceClosure callback);

 private:
  friend class SurfaceEmbedHostCollection;

  explicit SurfaceEmbedHost(SurfaceEmbedHostCollection* collection);

  void Bind(mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver);

  void OnMojoDisconnect();

  // May return null.
  content::SurfaceEmbedConnector* GetConnector() const;

  raw_ref<SurfaceEmbedHostCollection> collection_;
  base::OnceClosure destruction_callback_for_testing_;

  // The WebContents of the child document.
  base::WeakPtr<content::WebContents> child_contents_ = nullptr;

  mojo::Remote<mojom::SurfaceEmbed> surface_embed_;
  mojo::Receiver<mojom::SurfaceEmbedHost> receiver_{this};
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
