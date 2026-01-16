// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
#define COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/surface_embed/common/surface_embed.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/surface_embed_connector.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/viz/public/mojom/compositing/local_surface_id.mojom-forward.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace surface_embed {

class COMPONENT_EXPORT(SURFACE_EMBED) SurfaceEmbedHost
    : public mojom::SurfaceEmbedHost,
      public content::SurfaceEmbedConnector::Delegate {
 public:
  ~SurfaceEmbedHost() override;

  SurfaceEmbedHost(const SurfaceEmbedHost&) = delete;
  SurfaceEmbedHost& operator=(const SurfaceEmbedHost&) = delete;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<mojom::SurfaceEmbedHost> receiver);

  static size_t GetInstanceCountForTesting();
  static size_t GetAttachedInstanceCountForTesting();

  // mojom::SurfaceEmbedHost implementation:
  void SetSurfaceEmbed(mojo::PendingAssociatedRemote<mojom::SurfaceEmbed>
                           surface_embed) override;
  void AttachConnector(int64_t content_id) override;
  void DetachConnector() override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool is_visible) override;
  void SetFocus(bool focused, blink::mojom::FocusType focus_type) override;

  // content::SurfaceEmbedConnector::Delegate implementation:
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void UpdateLocalSurfaceIdFromChild(
      const ::viz::LocalSurfaceId& local_surface_id) override;
  void FocusInEmbedder(
      content::SurfaceEmbedConnector::FocusOperation focus_op) override;
  void ChildProcessGone() override;
  void DetachedByHost() override;
  bool IsAttachedForTesting() const override;

 private:
  explicit SurfaceEmbedHost(content::RenderFrameHost*);

  void OnSurfaceEmbedDisconnected();

  // May return null.
  content::SurfaceEmbedConnector* GetConnector();

  // Count of all alive instances for testing.
  static size_t instance_count_for_testing_;
  // Count of all alive and attached instance for testing.
  static size_t attached_instance_count_for_testing_;

  content::GlobalRenderFrameHostId render_frame_host_id_;
  base::WeakPtr<content::WebContents> guest_contents_ = nullptr;
  bool know_have_focus_ = false;

  mojo::AssociatedRemote<mojom::SurfaceEmbed> surface_embed_;
};

}  // namespace surface_embed

#endif  // COMPONENTS_SURFACE_EMBED_BROWSER_SURFACE_EMBED_HOST_H_
