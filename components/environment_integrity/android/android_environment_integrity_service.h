// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_
#define COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_

#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace environment_integrity {

class AndroidEnvironmentIntegrityService final
    : public content::DocumentService<
          blink::mojom::EnvironmentIntegrityService> {
 public:
  AndroidEnvironmentIntegrityService(
      const AndroidEnvironmentIntegrityService&) = delete;
  AndroidEnvironmentIntegrityService& operator=(
      const AndroidEnvironmentIntegrityService&) = delete;
  ~AndroidEnvironmentIntegrityService() override;

  // Factory method for creating an instance of this interface.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService>
          receiver);

  // blink::mojom::EnvironmentIntegrityService:
  void GetEnvironmentIntegrity(
      GetEnvironmentIntegrityCallback callback) override;

 private:
  AndroidEnvironmentIntegrityService(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService>
          receiver);

  void OnGetHandle(GetEnvironmentIntegrityCallback callback,
                   absl::optional<int64_t> maybe_handle);

  void GetIntegrityTokenForHandle(int64_t handle,
                                  GetEnvironmentIntegrityCallback callback);

  base::WeakPtrFactory<AndroidEnvironmentIntegrityService> weak_factory_{this};
};

}  // namespace environment_integrity

#endif  // COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_
