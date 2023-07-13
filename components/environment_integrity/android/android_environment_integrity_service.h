// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_
#define COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_

#include <memory>
#include "components/environment_integrity/android/android_environment_integrity_data_manager.h"
#include "components/environment_integrity/android/integrity_service_bridge.h"
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

  // Factory method for test that allow injection of an `IntegrityService`
  // instance to mock out the JNI calls.
  static void CreateForTest(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver,
      std::unique_ptr<IntegrityService> integrity_service);

  // blink::mojom::EnvironmentIntegrityService:
  void GetEnvironmentIntegrity(
      const std::vector<uint8_t>& content_binding,
      GetEnvironmentIntegrityCallback callback) override;

 private:
  // Internal request object to bundle parameters that need to be threaded
  // through multiple callbacks.
  struct EnvironmentIntegrityRequest {
    EnvironmentIntegrityRequest(GetEnvironmentIntegrityCallback,
                                const std::vector<uint8_t>& content_binding);
    ~EnvironmentIntegrityRequest();
    EnvironmentIntegrityRequest(EnvironmentIntegrityRequest&&);
    EnvironmentIntegrityRequest& operator=(EnvironmentIntegrityRequest&&);

    GetEnvironmentIntegrityCallback callback;
    std::vector<uint8_t> content_binding;
    int64_t handle;
    bool recreating_old_handle;
  };

  AndroidEnvironmentIntegrityService(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::EnvironmentIntegrityService> receiver,
      std::unique_ptr<IntegrityService> integrity_service);

  AndroidEnvironmentIntegrityDataManager* GetDataManager();

  // Callback for `AndroidEnvironmentIntegrityDataManager::GetHandle`.
  void OnGetHandle(EnvironmentIntegrityRequest request,
                   absl::optional<int64_t> maybe_handle);

  // Callback for `CreateIntegrityHandle`.
  void OnCreateHandle(EnvironmentIntegrityRequest request,
                      HandleCreationResult result);

  // Callback for `GetEnvironmentIntegrityToken`.
  void OnGetToken(EnvironmentIntegrityRequest request, GetTokenResult result);

  std::unique_ptr<IntegrityService> integrity_service_;

  base::WeakPtrFactory<AndroidEnvironmentIntegrityService> weak_factory_{this};
};

}  // namespace environment_integrity

#endif  // COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_ANDROID_ENVIRONMENT_INTEGRITY_SERVICE_H_
