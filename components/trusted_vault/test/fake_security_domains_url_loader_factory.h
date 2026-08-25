// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_URL_LOADER_FACTORY_H_
#define COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"

namespace network {
class TestURLLoaderFactory;
}  // namespace network

namespace trusted_vault {

class FakeSecurityDomainsServer;

// An in-memory SharedURLLoaderFactory adapter that routes network requests to a
// FakeSecurityDomainsServer without EmbeddedTestServer.
//
// When to use this: Prefer this over net::EmbeddedTestServer whenever testing
// with base::test::TaskEnvironment(TimeSource::MOCK_TIME).
//
// Why this is needed with MOCK_TIME:
// net::EmbeddedTestServer runs on an unmanaged native OS thread handling real
// loopback TCP sockets. Under TaskEnvironment(TimeSource::MOCK_TIME), when the
// main thread or ThreadPool workers are waiting for external socket I/O from
// EmbeddedTestServer, TaskEnvironment considers the managed queues idle and
// fast-forwards virtual time to the next pending delayed task (e.g. socket
// connect/read timeout timers), causing spurious ERR_TIMED_OUT errors.
//
// By handling requests completely in-memory via TestURLLoaderFactory and Mojo,
// all request and response processing occurs on TaskEnvironment-managed tasks.
class FakeSecurityDomainsURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  FakeSecurityDomainsURLLoaderFactory(FakeSecurityDomainsServer* server,
                                      const GURL& server_url);
  FakeSecurityDomainsURLLoaderFactory(
      const FakeSecurityDomainsURLLoaderFactory&) = delete;
  FakeSecurityDomainsURLLoaderFactory& operator=(
      const FakeSecurityDomainsURLLoaderFactory&) = delete;

  // SharedURLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

 private:
  ~FakeSecurityDomainsURLLoaderFactory() override;

  void OnInterceptRequest(const network::ResourceRequest& request);

  const raw_ptr<FakeSecurityDomainsServer> server_;
  const GURL server_url_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_TEST_FAKE_SECURITY_DOMAINS_URL_LOADER_FACTORY_H_
