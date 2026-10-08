// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_FAKE_EPHEMERAL_KEY_SERVER_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_FAKE_EPHEMERAL_KEY_SERVER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/sync/protocol/agile_encryption_keys.pb.h"
#include "components/sync/protocol/encryption.pb.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace sync_tab_context {

// Mimics behavior of the ephemeral key server. This class is designed to be
// used with `net::test_server::EmbeddedTestServer` via registration of
// `HandleRequest()`.
class FakeEphemeralKeyServer {
 public:
  static GURL GetServerURL(const GURL& base_url);

  explicit FakeEphemeralKeyServer(const GURL& base_url);
  FakeEphemeralKeyServer(const FakeEphemeralKeyServer&) = delete;
  FakeEphemeralKeyServer& operator=(const FakeEphemeralKeyServer&) = delete;
  ~FakeEphemeralKeyServer();

  // Handles request if it belongs to the ephemeral key server (identified by
  // request URL). Returns nullptr otherwise.
  // Unlike other methods of this class, which may be called on the main thread,
  // this method is called on the `EmbeddedTestServer` IO thread.
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& http_request);

  // Configures an HTTP error status code to return on subsequent requests, or
  // `std::nullopt` to resume returning valid ephemeral keys.
  void SetHttpErrorToReturn(std::optional<net::HttpStatusCode> http_error);

  // Decrypts and decompresses `encrypted_content` using the container key
  // wrapped inside `serialized_access_token` (which must have been encrypted
  // with an ephemeral key previously issued by this server). Returns
  // `std::nullopt` if the token is unknown/invalid or decryption/decompression
  // fails.
  std::optional<std::string> DecryptContent(
      std::string_view serialized_access_token,
      const sync_pb::EncryptedData& encrypted_content) const;

 private:
  const GURL server_url_;
  std::atomic<std::optional<net::HttpStatusCode>> http_error_to_return_;

  // This class is used on the main thread and on the `EmbeddedTestServer` IO
  // thread, so mutable state is protected by `lock_`.
  mutable base::Lock lock_;
  uint64_t next_server_token_id_ GUARDED_BY(lock_) = 0;
  base::flat_map<std::string, sync_pb::AgileSymmetricKeySet> issued_key_sets_
      GUARDED_BY(lock_);
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_FAKE_EPHEMERAL_KEY_SERVER_H_
