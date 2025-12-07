// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/fake_shared_url_loader_factory.h"

#include "base/notreached.h"

namespace chromecast {

void FakeSharedURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  test_url_loader_factory_.Clone(std::move(receiver));
}

void FakeSharedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  test_url_loader_factory_.CreateLoaderAndStart(
      std::move(loader), request_id, options, request, std::move(client),
      traffic_annotation);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
FakeSharedURLLoaderFactory::Clone() {
  NOTREACHED();
}

FakePendingSharedURLLoaderFactory::FakePendingSharedURLLoaderFactory()
    : fake_shared_url_loader_factory_(
          base::MakeRefCounted<FakeSharedURLLoaderFactory>()) {}

FakePendingSharedURLLoaderFactory::~FakePendingSharedURLLoaderFactory() {}

scoped_refptr<network::SharedURLLoaderFactory>
FakePendingSharedURLLoaderFactory::CreateFactory() {
  return fake_shared_url_loader_factory_;
}

}  // namespace chromecast
