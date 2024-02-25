// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_HTTP_POST_PROVIDER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_HTTP_POST_PROVIDER_H_

#include <atomic>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/net/http_post_provider.h"
#include "components/sync/engine/net/http_post_provider_factory.h"

namespace fake_server {

class FakeServer;

class FakeServerHttpPostProvider : public syncer::HttpPostProvider {
 public:
  FakeServerHttpPostProvider(
      const base::WeakPtr<FakeServer>& fake_server,
      scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner);

  FakeServerHttpPostProvider(const FakeServerHttpPostProvider&) = delete;
  FakeServerHttpPostProvider& operator=(const FakeServerHttpPostProvider&) =
      delete;

  // HttpPostProvider implementation.
  void SetExtraRequestHeaders(const char* headers) override;
  void SetURL(const GURL& url) override;
  void SetPostPayload(const char* content_type,
                      int content_length,
                      const char* content) override;
  bool MakeSynchronousPost(int* net_error_code, int* http_status_code) override;
  void Abort() override;
  int GetResponseContentLength() const override;
  const char* GetResponseContent() const override;
  const std::string GetResponseHeaderValue(
      const std::string& name) const override;

  // Forces every request to fail in a way that simulates a network failure.
  // This can be used to trigger exponential backoff in the client.
  static void DisableNetwork();

  // Undoes the effects of DisableNetwork.
  static void EnableNetwork();

 protected:
  ~FakeServerHttpPostProvider() override;

 private:
  friend class base::RefCountedThreadSafe<FakeServerHttpPostProvider>;

  void HandleCommandOnFakeServerThread(int* http_status_code,
                                       std::string* response);

  static std::atomic_bool network_enabled_;

  // |fake_server_| should only be dereferenced on the same thread as
  // |fake_server_task_runner_| runs on.
  const base::WeakPtr<FakeServer> fake_server_;
  const scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner_;

  base::WaitableEvent synchronous_post_completion_ =
      base::WaitableEvent(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                          base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::atomic_bool aborted_ = false;

  std::string response_;
  GURL request_url_;
  std::string request_content_;
  std::string request_content_type_;
  std::string extra_request_headers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class FakeServerHttpPostProviderFactory
    : public syncer::HttpPostProviderFactory {
 public:
  FakeServerHttpPostProviderFactory(
      const base::WeakPtr<FakeServer>& fake_server,
      scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner);

  FakeServerHttpPostProviderFactory(const FakeServerHttpPostProviderFactory&) =
      delete;
  FakeServerHttpPostProviderFactory& operator=(
      const FakeServerHttpPostProviderFactory&) = delete;

  ~FakeServerHttpPostProviderFactory() override;

  // HttpPostProviderFactory:
  scoped_refptr<syncer::HttpPostProvider> Create() override;

 private:
  // |fake_server_| should only be dereferenced on the same thread as
  // |fake_server_task_runner_| runs on.
  base::WeakPtr<FakeServer> fake_server_;
  scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_HTTP_POST_PROVIDER_H_
