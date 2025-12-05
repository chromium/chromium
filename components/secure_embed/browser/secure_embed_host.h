// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
#define COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/secure_embed/common/secure_embed.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/secure_embed_connector.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "services/viz/public/mojom/compositing/local_surface_id.mojom-forward.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace secure_embed {

class COMPONENT_EXPORT(SECURE_EMBED) SecureEmbedHost
    : public mojom::SecureEmbedHost,
      public content::SecureEmbedConnector::Delegate {
 public:
  ~SecureEmbedHost() override;

  SecureEmbedHost(const SecureEmbedHost&) = delete;
  SecureEmbedHost& operator=(const SecureEmbedHost&) = delete;

  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingAssociatedReceiver<mojom::SecureEmbedHost> receiver);

  static size_t GetInstanceCountForTesting();
  static size_t GetAttachedInstanceCountForTesting();

  // mojom::SecureEmbedHost implementation:
  void SetSecureEmbed(
      mojo::PendingAssociatedRemote<mojom::SecureEmbed> secure_embed) override;
  void AttachConnector(int64_t content_id) override;
  void DetachConnector() override;
  void SynchronizeVisualProperties(
      const blink::FrameVisualProperties& visual_properties,
      bool is_visible) override;
  void SetFocus(bool focused, blink::mojom::FocusType focus_type) override;

  // content::SecureEmbedConnector::Delegate implementation:
  void SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void UpdateLocalSurfaceIdFromChild(
      const ::viz::LocalSurfaceId& local_surface_id) override;
  void FocusInEmbedder(
      content::SecureEmbedConnector::FocusOperation focus_op) override;
  void ChildProcessGone() override;
  void DetachedByHost() override;
  content::RenderFrameHost* ParentFrame() override;
  bool IsAttachedForTesting() const override;

 private:
  explicit SecureEmbedHost(content::RenderFrameHost*);

  void OnSecureEmbedDisconnected();

  // May return null.
  content::SecureEmbedConnector* GetConnector();

  // Count of all alive instances for testing.
  static size_t instance_count_for_testing_;
  // Count of all alive and attached instance for testing.
  static size_t attached_instance_count_for_testing_;

  content::GlobalRenderFrameHostId render_frame_host_id_;
  base::WeakPtr<content::WebContents> guest_contents_ = nullptr;
  bool know_have_focus_ = false;

  mojo::AssociatedRemote<mojom::SecureEmbed> secure_embed_;
};

}  // namespace secure_embed

#endif  // COMPONENTS_SECURE_EMBED_BROWSER_SECURE_EMBED_HOST_H_
