// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_server_http_post_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "components/sync/test/fake_server.h"
#include "net/base/net_errors.h"

namespace fake_server {

// static
std::atomic_bool FakeServerHttpPostProvider::network_enabled_(true);

FakeServerHttpPostProviderFactory::FakeServerHttpPostProviderFactory(
    const base::WeakPtr<FakeServer>& fake_server,
    scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner)
    : fake_server_(fake_server),
      fake_server_task_runner_(fake_server_task_runner) {}

FakeServerHttpPostProviderFactory::~FakeServerHttpPostProviderFactory() =
    default;

scoped_refptr<syncer::HttpPostProvider>
FakeServerHttpPostProviderFactory::Create() {
  return new FakeServerHttpPostProvider(fake_server_, fake_server_task_runner_);
}

FakeServerHttpPostProvider::FakeServerHttpPostProvider(
    const base::WeakPtr<FakeServer>& fake_server,
    scoped_refptr<base::SequencedTaskRunner> fake_server_task_runner)
    : fake_server_(fake_server),
      fake_server_task_runner_(fake_server_task_runner) {}

FakeServerHttpPostProvider::~FakeServerHttpPostProvider() = default;

void FakeServerHttpPostProvider::SetExtraRequestHeaders(const char* headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(pvalenzuela): Add assertions on this value.
  extra_request_headers_.assign(headers);
}

void FakeServerHttpPostProvider::SetURL(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request_url_ = url;
}

void FakeServerHttpPostProvider::SetPostPayload(const char* content_type,
                                                int content_length,
                                                const char* content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request_content_type_.assign(content_type);
  request_content_.assign(content, content_length);
}

bool FakeServerHttpPostProvider::MakeSynchronousPost(int* net_error_code,
                                                     int* http_status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!network_enabled_) {
    response_.clear();
    *net_error_code = net::ERR_INTERNET_DISCONNECTED;
    *http_status_code = 0;
    return false;
  }

  synchronous_post_completion_.Reset();
  aborted_ = false;

  // It is assumed that a POST is being made to /command.
  int post_status_code = -1;
  std::string post_response;

  bool result = fake_server_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeServerHttpPostProvider::HandleCommandOnFakeServerThread,
          base::RetainedRef(this), base::Unretained(&post_status_code),
          base::Unretained(&post_response)));

  if (!result) {
    response_.clear();
    *net_error_code = net::ERR_UNEXPECTED;
    *http_status_code = 0;
    return false;
  }

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    synchronous_post_completion_.Wait();
  }

  if (aborted_) {
    *net_error_code = net::ERR_ABORTED;
    return false;
  }

  // Zero means success.
  *net_error_code = 0;
  *http_status_code = post_status_code;
  response_ = post_response;

  return true;
}

int FakeServerHttpPostProvider::GetResponseContentLength() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_.length();
}

const char* FakeServerHttpPostProvider::GetResponseContent() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return response_.c_str();
}

const std::string FakeServerHttpPostProvider::GetResponseHeaderValue(
    const std::string& name) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string();
}

void FakeServerHttpPostProvider::Abort() {
  // Note: This may be called on any thread, so no |sequence_checker_| here.
  // The sync thread could be blocked in MakeSynchronousPost(), waiting
  // for HandleCommandOnFakeServerThread() to be processed and completed.
  // This causes an immediate unblocking which will be returned as
  // net::ERR_ABORTED.
  aborted_ = true;
  synchronous_post_completion_.Signal();
}

// static
void FakeServerHttpPostProvider::DisableNetwork() {
  // Note: This may be called on any thread.
  network_enabled_ = false;
}

// static
void FakeServerHttpPostProvider::EnableNetwork() {
  // Note: This may be called on any thread.
  network_enabled_ = true;
}

void FakeServerHttpPostProvider::HandleCommandOnFakeServerThread(
    int* http_status_code,
    std::string* response) {
  DCHECK(fake_server_task_runner_->RunsTasksInCurrentSequence());

  if (!fake_server_ || aborted_) {
    // Command explicitly aborted or server destroyed.
    return;
  }

  *http_status_code = fake_server_->HandleCommand(request_content_, response);
  synchronous_post_completion_.Signal();
}

}  // namespace fake_server
