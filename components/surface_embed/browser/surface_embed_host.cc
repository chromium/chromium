// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/browser/surface_embed_host.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/guest_contents/browser/guest_contents_handle.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/surface_embed_connector.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"

namespace surface_embed {

// Owns all the SurfaceEmbedHost instances for a given document.
class SurfaceEmbedHostCollection
    : public content::DocumentUserData<SurfaceEmbedHostCollection> {
 public:
  ~SurfaceEmbedHostCollection() override;

  SurfaceEmbedHostCollection(const SurfaceEmbedHostCollection&) = delete;
  SurfaceEmbedHostCollection& operator=(const SurfaceEmbedHostCollection&) =
      delete;

  // Creates a new SurfaceEmbedHost and adds it to this collection.
  SurfaceEmbedHost* CreateHost();
  void RemoveHost(SurfaceEmbedHost* host);

 private:
  friend class content::DocumentUserData<SurfaceEmbedHostCollection>;

  explicit SurfaceEmbedHostCollection(content::RenderFrameHost* rfh);

  absl::flat_hash_set<std::unique_ptr<SurfaceEmbedHost>> hosts_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

DOCUMENT_USER_DATA_KEY_IMPL(SurfaceEmbedHostCollection);

SurfaceEmbedHostCollection::SurfaceEmbedHostCollection(
    content::RenderFrameHost* rfh)
    : DocumentUserData<SurfaceEmbedHostCollection>(rfh) {}

SurfaceEmbedHostCollection::~SurfaceEmbedHostCollection() = default;

SurfaceEmbedHost* SurfaceEmbedHostCollection::CreateHost() {
  auto host = base::WrapUnique(new SurfaceEmbedHost(this));
  SurfaceEmbedHost* host_ptr = host.get();
  hosts_.insert(std::move(host));
  return host_ptr;
}

void SurfaceEmbedHostCollection::RemoveHost(SurfaceEmbedHost* host) {
  auto it = hosts_.find(host);
  CHECK(it != hosts_.end());
  hosts_.erase(it);
}

// static
SurfaceEmbedHost* SurfaceEmbedHost::Create(
    content::RenderFrameHost* rfh,
    mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver) {
  auto* collection =
      SurfaceEmbedHostCollection::GetOrCreateForCurrentDocument(rfh);
  SurfaceEmbedHost* host = collection->CreateHost();
  host->Bind(std::move(receiver));
  return host;
}

SurfaceEmbedHost::SurfaceEmbedHost(SurfaceEmbedHostCollection* collection)
    : collection_(*collection) {}

SurfaceEmbedHost::~SurfaceEmbedHost() {
  DetachConnector();
  if (destruction_callback_for_testing_) {
    std::move(destruction_callback_for_testing_).Run();
  }
}

void SurfaceEmbedHost::Bind(
    mojo::PendingReceiver<mojom::SurfaceEmbedHost> receiver) {
  CHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SurfaceEmbedHost::OnMojoDisconnect, base::Unretained(this)));
}

void SurfaceEmbedHost::SetDestructionCallbackForTesting(
    base::OnceClosure callback) {
  destruction_callback_for_testing_ = std::move(callback);
}

void SurfaceEmbedHost::SetSurfaceEmbed(
    mojo::PendingRemote<mojom::SurfaceEmbed> surface_embed) {
  CHECK(!surface_embed_.is_bound());
  surface_embed_.Bind(std::move(surface_embed));
  surface_embed_.set_disconnect_handler(base::BindOnce(
      &SurfaceEmbedHost::OnMojoDisconnect, base::Unretained(this)));
}

void SurfaceEmbedHost::AttachConnector(
    const base::UnguessableToken& content_id) {
  // Should never call attach without having a valid SurfaceEmbed remote already
  // bound.
  CHECK(surface_embed_);

  CHECK(!content_id.is_empty());
  guest_contents::GuestContentsHandle* guest_handle =
      guest_contents::GuestContentsHandle::FromID(content_id);
  CHECK(guest_handle);

  content::WebContents* web_contents_to_attach = guest_handle->web_contents();
  CHECK(web_contents_to_attach);

  // If the child WebContents is already attached to a SurfaceEmbedConnector, we
  // need to detach it first. Since we're detaching some other SurfaceEmbedHost,
  // we need to notify it of the detachment so the host and
  // SurfaceEmbedWebPlugin stay in sync.
  if (auto* connector = web_contents_to_attach->GetSurfaceEmbedConnector()) {
    connector->GetDelegate()->DetachedByHost();
    CHECK(!web_contents_to_attach->GetSurfaceEmbedConnector());
  }

  // If this host already has a child attached, we need to detach it first. Note
  // that this request comes from the parent side, so we don't notify the
  // SurfaceEmbed as it initiated the detachment.
  DetachConnector();

  if (web_contents_to_attach->IsCrashed()) {
    // The child process may have crashed before the renderer for the parent
    // got the chance to attach it. We want to handle this early since attaching
    // the connector will make WebContents think the process is alive.
    surface_embed_->ChildProcessGone();
    return;
  }

  child_contents_ = web_contents_to_attach->GetWeakPtr();
  content::WebContents* parent_web_contents =
      content::WebContents::FromRenderFrameHost(
          &collection_->render_frame_host());
  content::SurfaceEmbedConnector::Attach(web_contents_to_attach,
                                         parent_web_contents, this);

  auto* connector = GetConnector();
  CHECK(connector->GetFrameSinkId().is_valid());
  surface_embed_->SetFrameSinkId(connector->GetFrameSinkId());

  // TODO(surface-embed): If accessibility info was received before the
  // connector was attached, pass it to the connector now.
}

void SurfaceEmbedHost::DetachConnector() {
  if (GetConnector()) {
    content::SurfaceEmbedConnector::Detach(child_contents_.get());
    child_contents_ = nullptr;
  }
}

void SurfaceEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool is_visible) {
  CHECK(surface_embed_.is_bound());

  if (content::SurfaceEmbedConnector* connector = GetConnector()) {
    connector->OnVisibilityChanged(
        is_visible ? blink::mojom::FrameVisibility::kRenderedInViewport
                   : blink::mojom::FrameVisibility::kNotRendered);
    connector->OnSynchronizeVisualProperties(visual_properties);
  }
}

void SurfaceEmbedHost::SetFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  if (surface_embed_) {
    surface_embed_->SetFrameSinkId(frame_sink_id);
  }
}

void SurfaceEmbedHost::UpdateLocalSurfaceIdFromChild(
    const viz::LocalSurfaceId& local_surface_id) {
  if (surface_embed_) {
    surface_embed_->UpdateLocalSurfaceIdFromChild(local_surface_id);
  }
}

void SurfaceEmbedHost::ChildProcessGone() {
  if (surface_embed_) {
    surface_embed_->ChildProcessGone();
  }
}

void SurfaceEmbedHost::DetachedByHost() {
  // We're being forcibly detached (child being re-attached elsewhere).
  CHECK(child_contents_);

  // TODO(surface-embed): Notify the renderer's SurfaceEmbedWebPlugin that the
  // host initiated the detachment.

  DetachConnector();
}

void SurfaceEmbedHost::RequestFocus() {
  if (surface_embed_) {
    surface_embed_->RequestFocus();
  }
}

bool SurfaceEmbedHost::IsAttachedForTesting() const {
  return child_contents_ != nullptr;
}

void SurfaceEmbedHost::OnMojoDisconnect() {
  // Reset both pipes. This will prevent the other disconnect handler from being
  // called if it was also scheduled to run. The first one wins.
  receiver_.reset();
  surface_embed_.reset();
  collection_->RemoveHost(this);
}

content::SurfaceEmbedConnector* SurfaceEmbedHost::GetConnector() const {
  return child_contents_ ? child_contents_->GetSurfaceEmbedConnector()
                         : nullptr;
}

}  // namespace surface_embed
