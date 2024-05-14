// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/fwupd/fake_fwupd_download_client.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {

class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() = default;
  FakeSharedURLLoaderFactory(const FakeSharedURLLoaderFactory&) = delete;
  FakeSharedURLLoaderFactory& operator=(const FakeSharedURLLoaderFactory&) =
      delete;

  // network::mojom::URLLoaderFactory implementation:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    test_url_loader_factory_.Clone(std::move(receiver));
  }

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    test_url_loader_factory_.CreateLoaderAndStart(
        std::move(loader), request_id, options, request, std::move(client),
        traffic_annotation);
  }

  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  friend class base::RefCounted<FakeSharedURLLoaderFactory>;

  ~FakeSharedURLLoaderFactory() override = default;

  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace

namespace ash {

FakeFwupdDownloadClient::FakeFwupdDownloadClient()
    : url_loader_factory_(base::MakeRefCounted<FakeSharedURLLoaderFactory>()) {}
FakeFwupdDownloadClient::~FakeFwupdDownloadClient() = default;

scoped_refptr<network::SharedURLLoaderFactory>
FakeFwupdDownloadClient::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::TestURLLoaderFactory&
FakeFwupdDownloadClient::test_url_loader_factory() {
  return static_cast<FakeSharedURLLoaderFactory*>(url_loader_factory_.get())
      ->test_url_loader_factory();
}

}  // namespace ash