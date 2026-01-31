// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_EMBEDDED_FAKE_SERVER_ADAPTER_H_
#define COMPONENTS_SYNC_TEST_EMBEDDED_FAKE_SERVER_ADAPTER_H_

#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "net/test/embedded_test_server/http_response.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace net::test_server {
class EmbeddedTestServer;
struct HttpRequest;
}  // namespace net::test_server

namespace fake_server {

class FakeServer;

// Adapter class that allows hooking FakeServer with EmbeddedTestServer,
// implementing a realistic HTTP[S] endpoint implementing the Sync protocol. It
// internally deals with the multi-threading considerations of using
// EmbeddedTestServer.
class EmbeddedFakeServerAdapter {
 public:
  static constexpr std::string_view kPath = "/chrome-sync";

  explicit EmbeddedFakeServerAdapter(base::WeakPtr<FakeServer> fake_server);
  EmbeddedFakeServerAdapter(const EmbeddedFakeServerAdapter&) = delete;
  ~EmbeddedFakeServerAdapter();

  EmbeddedFakeServerAdapter& operator=(const EmbeddedFakeServerAdapter&) =
      delete;

  // Registers handlers in `embedded_test_server` to offer an HTTP[S] endpoint
  // implementing the Sync HTTP protocol for `this`. The path under which the
  // server is installed is determined by `kPath`.
  void RegisterRequestHandler(
      net::test_server::EmbeddedTestServer& embedded_test_server);

 private:
  // Simple data structure to allow one thread to pass the response from one
  // thread to another. The receiving thread constructs the object and then
  // blocks until the other thread populates the fields.
  struct PendingResponse : public base::RefCountedThreadSafe<PendingResponse> {
    PendingResponse();

    std::unique_ptr<net::test_server::BasicHttpResponse> response;
    base::WaitableEvent completion_event;

   private:
    friend class base::RefCountedThreadSafe<PendingResponse>;

    ~PendingResponse();
  };

  // Data structure to track in-flight requests by EmbeddedTestServer, for the
  // purpose of aborting during shutdown.
  class PendingResponseTracker
      : public base::RefCountedThreadSafe<PendingResponseTracker> {
   public:
    PendingResponseTracker();

    // Invoked during shutdown, after which no further requests should be
    // accepted.
    void Abort();

    // Returns whether `Abort()` has been invoked.
    bool IsAborted() const;

    // If Abort() has already been invoked, it immediately signals `event`.
    // Otherwise, it keeps a reference to `event` until `Abort()` is invoked,
    // at which point `event` will be signaled.
    void SignalEventWhenAbortedOrNow(base::WaitableEvent* event);

    // Untracks a previously added event. No-op if `event` isn't tracked.
    void RemoveEvent(base::WaitableEvent* event);

   private:
    friend class base::RefCountedThreadSafe<PendingResponseTracker>;

    ~PendingResponseTracker();

    mutable base::Lock lock_;
    bool aborted_ GUARDED_BY(lock_) = false;
    base::flat_set<raw_ptr<base::WaitableEvent>> events_ GUARDED_BY(lock_);
  };

  // Function exercised by EmbeddedTestServer on the backend thread (IO thread).
  static std::unique_ptr<net::test_server::HttpResponse>
  HandleRequestOnBackendThread(
      base::WeakPtr<FakeServer> fake_server,
      scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner,
      scoped_refptr<PendingResponseTracker> pending_response_tracker,
      const net::test_server::HttpRequest& request);

  // Function for command requests specifically that runs in FakeServer's
  // sequence (UI thread).
  static void HandleCommandRequestOnFakeServerSequence(
      base::WeakPtr<FakeServer> fake_server,
      const std::string& compressed_request_content,
      scoped_refptr<PendingResponse> response);

  const base::WeakPtr<FakeServer> fake_server_;
  const scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner_;
  const scoped_refptr<PendingResponseTracker> pending_response_tracker_;
};

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_EMBEDDED_FAKE_SERVER_ADAPTER_H_
