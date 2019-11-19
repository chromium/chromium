// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/camera/arc_camera_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/dbus/arc_camera_client.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/session/arc_bridge_service.h"
#include "crypto/random.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

namespace {

// Singleton factory for ArcCameraBridge.
class ArcCameraBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcCameraBridge,
          ArcCameraBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcCameraBridgeFactory";

  static ArcCameraBridgeFactory* GetInstance() {
    return base::Singleton<ArcCameraBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcCameraBridgeFactory>;
  ArcCameraBridgeFactory() = default;
  ~ArcCameraBridgeFactory() override = default;
};

}  // namespace

// Runs the callback after verifying the connection to the service.
class ArcCameraBridge::PendingStartCameraServiceResult {
 public:
  PendingStartCameraServiceResult(
      ArcCameraBridge* owner,
      mojo::ScopedMessagePipeHandle pipe,
      ArcCameraBridge::StartCameraServiceCallback callback)
      : owner_(owner),
        service_(mojom::CameraServicePtrInfo(std::move(pipe), 0u)),
        callback_(std::move(callback)) {
    service_.set_connection_error_handler(
        base::Bind(&PendingStartCameraServiceResult::OnError,
                   weak_ptr_factory_.GetWeakPtr()));
    service_.QueryVersion(
        base::Bind(&PendingStartCameraServiceResult::OnVersionReady,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  ~PendingStartCameraServiceResult() = default;

 private:
  void OnVersionReady(uint32_t version) { Finish(); }

  void OnError() {
    LOG(ERROR) << "Failed to query the camera service version.";
    // Run the callback anyways. The same error will be delivered to the Android
    // side error handler.
    Finish();
  }

  // Runs the callback and removes this object from the owner.
  void Finish() {
    DCHECK(callback_);
    std::move(callback_).Run(std::move(service_));
    // Destructs |this|.
    owner_->pending_start_camera_service_results_.erase(this);
  }

  ArcCameraBridge* const owner_;
  mojom::CameraServicePtr service_;
  ArcCameraBridge::StartCameraServiceCallback callback_;
  base::WeakPtrFactory<PendingStartCameraServiceResult> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PendingStartCameraServiceResult);
};

// static
ArcCameraBridge* ArcCameraBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcCameraBridgeFactory::GetForBrowserContext(context);
}

ArcCameraBridge::ArcCameraBridge(content::BrowserContext* context,
                                 ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->camera()->SetHost(this);
}

ArcCameraBridge::~ArcCameraBridge() {
  arc_bridge_service_->camera()->SetHost(nullptr);
}

void ArcCameraBridge::StartCameraService(StartCameraServiceCallback callback) {
  char random_bytes[16];
  crypto::RandBytes(random_bytes, 16);
  std::string token = base::HexEncode(random_bytes, 16);

  mojo::OutgoingInvitation invitation;
  mojo::PlatformChannel channel;
  mojo::ScopedMessagePipeHandle server_pipe =
      invitation.AttachMessagePipe(token);

  // Run the callback after verifying the connection to the service process.
  auto pending_result = std::make_unique<PendingStartCameraServiceResult>(
      this, std::move(server_pipe), std::move(callback));
  auto* pending_result_ptr = pending_result.get();
  pending_start_camera_service_results_[pending_result_ptr] =
      std::move(pending_result);

  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle,
                                 channel.TakeLocalEndpoint());
  base::ScopedFD fd =
      channel.TakeRemoteEndpoint().TakePlatformHandle().TakeFD();

  chromeos::ArcCameraClient::Get()->StartService(
      fd.get(), token, base::BindOnce([](bool success) {}));
}

void ArcCameraBridge::RegisterCameraHalClient(
    mojo::PendingRemote<cros::mojom::CameraHalClient> client) {
  media::CameraHalDispatcherImpl::GetInstance()->RegisterClient(
      std::move(client));
}

}  // namespace arc
