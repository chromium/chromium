// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/surface_embed/browser/dummy_surface_provider.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
}

namespace surface_embed {

class SurfaceEmbedHostCollection;

// The browser process counterpart to the SurfaceEmbedWebPlugin. This class
// bridges the plugin in the embedder page and the SecureEmbedConnector owned by
// the child WebContents. It provides the plugin with the surface of the child
// WebContents and synchronizes visual properties with the connector.
class SurfaceEmbedHost : public mojom::SurfaceEmbedHost {
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
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool is_visible) override;

  void SetDestructionCallbackForTesting(base::OnceClosure callback);

 private:
  friend class SurfaceEmbedHostCollection;

  explicit SurfaceEmbedHost(SurfaceEmbedHostCollection* collection);

  void Bind(mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver);

  void OnMojoDisconnect();

  raw_ref<SurfaceEmbedHostCollection> collection_;
  // TODO(surface-embed): Retrieve the surface from the child WebContents.
  std::unique_ptr<DummySurfaceProvider> dummy_surface_provider_;
  base::OnceClosure destruction_callback_for_testing_;

  mojo::Remote<mojom::SurfaceEmbed> surface_embed_;
  mojo::Receiver<mojom::SurfaceEmbedHost> receiver_{this};
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
