// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/embedded_fake_server_adapter.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "components/sync/test/fake_server.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/zlib/google/compression_utils.h"

namespace fake_server {

namespace {

constexpr std::string_view kCommandPath = "/chrome-sync/command/";
constexpr std::string_view kEventPath = "/chrome-sync/event/";

}  // namespace

EmbeddedFakeServerAdapter::PendingResponse::PendingResponse() = default;

EmbeddedFakeServerAdapter::PendingResponse::~PendingResponse() = default;

EmbeddedFakeServerAdapter::PendingResponseTracker::PendingResponseTracker() =
    default;

EmbeddedFakeServerAdapter::PendingResponseTracker::~PendingResponseTracker() =
    default;

void EmbeddedFakeServerAdapter::PendingResponseTracker::Abort() {
  base::AutoLock auto_lock(lock_);
  aborted_ = true;
  for (base::WaitableEvent* event : events_) {
    CHECK(event);
    event->Signal();
  }
}

bool EmbeddedFakeServerAdapter::PendingResponseTracker::IsAborted() const {
  base::AutoLock auto_lock(lock_);
  return aborted_;
}

void EmbeddedFakeServerAdapter::PendingResponseTracker::
    SignalEventWhenAbortedOrNow(base::WaitableEvent* event) {
  CHECK(event);

  base::AutoLock auto_lock(lock_);
  if (aborted_) {
    event->Signal();
  } else {
    events_.insert(event);
  }
}

void EmbeddedFakeServerAdapter::PendingResponseTracker::RemoveEvent(
    base::WaitableEvent* event) {
  base::AutoLock auto_lock(lock_);
  events_.erase(event);
}

EmbeddedFakeServerAdapter::EmbeddedFakeServerAdapter(
    base::WeakPtr<FakeServer> fake_server)
    : fake_server_(fake_server),
      fake_server_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      pending_response_tracker_(
          base::MakeRefCounted<PendingResponseTracker>()) {}

EmbeddedFakeServerAdapter::~EmbeddedFakeServerAdapter() {
  pending_response_tracker_->Abort();
}

void EmbeddedFakeServerAdapter::RegisterRequestHandler(
    net::test_server::EmbeddedTestServer& embedded_test_server) {
  embedded_test_server.RegisterRequestHandler(base::BindRepeating(
      &EmbeddedFakeServerAdapter::HandleRequestOnBackendThread, fake_server_,
      fake_server_task_runner_, pending_response_tracker_));
}

// static
std::unique_ptr<net::test_server::HttpResponse>
EmbeddedFakeServerAdapter::HandleRequestOnBackendThread(
    base::WeakPtr<FakeServer> fake_server,
    scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner,
    scoped_refptr<PendingResponseTracker> pending_response_tracker,
    const net::test_server::HttpRequest& request) {
  // WARNING: `fake_server` should not be dereferenced in this function, as this
  // may only be done in the frontend sequence.
  CHECK(fake_server_task_runner);
  CHECK(pending_response_tracker);

  if (request.GetURL().path() == kEventPath) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("ok");
    return response;
  }

  if (request.GetURL().path() != kCommandPath) {
    return nullptr;
  }

  // Use a WaitableEvent that is signaled as soon as the operation completes or
  // EmbeddedFakeServerAdapter is destroyed (tracker aborted), whichever happens
  // first.
  auto pending_response = base::MakeRefCounted<PendingResponse>();
  pending_response->response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  pending_response_tracker->SignalEventWhenAbortedOrNow(
      &pending_response->completion_event);

  // Post the actual task to handle the command in the UI thread. Note that this
  // is done regardless of shutdown state, because if FakeServer is destroyed it
  // is handled gracefully.
  fake_server_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&HandleCommandRequestOnFakeServerSequence, fake_server,
                     request.content, pending_response));

  // Block until network request completes or is aborted.
  base::ScopedAllowBlockingForTesting allow_wait;
  pending_response->completion_event.Wait();
  pending_response_tracker->RemoveEvent(&pending_response->completion_event);

  // If the tracker was aborted, the request may or may not have completed. For
  // simplicity and to avoid race conditions, return a fixed response
  // independent from `response`.
  if (pending_response_tracker->IsAborted()) {
    auto aborted_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    aborted_response->set_code(net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE);
    return aborted_response;
  }

  // If the tracker wasn't aborted, it means
  // `HandleCommandRequestOnFakeServerSequence` must have completed for this
  // request.
  return std::move(pending_response->response);
}

// static
void EmbeddedFakeServerAdapter::HandleCommandRequestOnFakeServerSequence(
    base::WeakPtr<FakeServer> fake_server,
    const std::string& compressed_request_content,
    scoped_refptr<PendingResponse> pending_response) {
  CHECK(pending_response);

  if (!fake_server) {
    pending_response->response->set_code(
        net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE);
    pending_response->completion_event.Signal();
    return;
  }

  std::string uncompressed_request_content;
  if (!compression::GzipUncompress(compressed_request_content,
                                   &uncompressed_request_content)) {
    pending_response->response->set_code(
        net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
    pending_response->completion_event.Signal();
    return;
  }

  std::string response_content;
  pending_response->response->set_code(fake_server->HandleCommand(
      uncompressed_request_content, &response_content));
  pending_response->response->set_content(response_content);
  pending_response->completion_event.Signal();
}

}  // namespace fake_server
