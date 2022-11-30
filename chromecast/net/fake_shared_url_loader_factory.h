// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_FAKE_SHARED_URL_LOADER_FACTORY_H_
#define CHROMECAST_NET_FAKE_SHARED_URL_LOADER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace chromecast {

// A simple SharedURLLoaderFactory implementation for tests.
class FakeSharedURLLoaderFactory final
    : public network::SharedURLLoaderFactory {
 public:
  FakeSharedURLLoaderFactory() = default;

  // network::mojom::URLLoaderFactory implementation:
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  // network::SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  network::TestURLLoaderFactory& test_url_loader_factory() {
    return test_url_loader_factory_;
  }

 private:
  friend class base::RefCounted<FakeSharedURLLoaderFactory>;

  ~FakeSharedURLLoaderFactory() override = default;

  network::TestURLLoaderFactory test_url_loader_factory_;

  FakeSharedURLLoaderFactory(const FakeSharedURLLoaderFactory&) = delete;
  FakeSharedURLLoaderFactory& operator=(const FakeSharedURLLoaderFactory&) =
      delete;
};

// A simple PendingSharedURLLoaderFactory implementation for tests.
class FakePendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  FakePendingSharedURLLoaderFactory();
  ~FakePendingSharedURLLoaderFactory() override;

  scoped_refptr<FakeSharedURLLoaderFactory> fake_shared_url_loader_factory() {
    return fake_shared_url_loader_factory_;
  }

 private:
  friend class network::SharedURLLoaderFactory;

  // network::PendingSharedURLLoaderFactory implementation:
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  scoped_refptr<FakeSharedURLLoaderFactory> fake_shared_url_loader_factory_;

  FakePendingSharedURLLoaderFactory(const FakePendingSharedURLLoaderFactory&) =
      delete;
  FakePendingSharedURLLoaderFactory& operator=(
      const FakePendingSharedURLLoaderFactory&) = delete;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_FAKE_SHARED_URL_LOADER_FACTORY_H_
