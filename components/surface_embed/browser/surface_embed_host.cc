// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surface_embed/browser/surface_embed_host.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/surface_embed/browser/dummy_surface_provider.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
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
    : collection_(*collection),
      dummy_surface_provider_(std::make_unique<DummySurfaceProvider>()) {}

SurfaceEmbedHost::~SurfaceEmbedHost() {
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
  surface_embed_->SetFrameSinkId(dummy_surface_provider_->frame_sink_id());
}

void SurfaceEmbedHost::SynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties,
    bool is_visible) {
  CHECK(surface_embed_.is_bound());
  if (!is_visible) {
    return;
  }
  dummy_surface_provider_->SubmitCompositorFrame(
      visual_properties.local_surface_id,
      visual_properties.screen_infos.current().device_scale_factor,
      visual_properties.local_frame_size);
}

void SurfaceEmbedHost::OnMojoDisconnect() {
  // Reset both pipes. This will prevent the other disconnect handler from being
  // called if it was also scheduled to run. The first one wins.
  receiver_.reset();
  surface_embed_.reset();
  collection_->RemoveHost(this);
}

}  // namespace surface_embed
