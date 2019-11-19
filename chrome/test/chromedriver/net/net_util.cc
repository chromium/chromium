// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/net_util.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace {

base::LazyInstance<scoped_refptr<base::SequencedTaskRunner>>::Leaky
    g_io_capable_task_runner_for_tests = LAZY_INSTANCE_INITIALIZER;

class SyncUrlFetcher {
 public:
  SyncUrlFetcher(const GURL& url,
                 network::mojom::URLLoaderFactory* url_loader_factory,
                 std::string* response)
      : url_(url),
        url_loader_factory_(url_loader_factory),
        network_task_runner_(g_io_capable_task_runner_for_tests.Get()
                                 ? g_io_capable_task_runner_for_tests.Get()
                                 : base::CreateSequencedTaskRunner(
                                       {base::ThreadPool(), base::MayBlock()})),
        response_(response),
        event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
               base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~SyncUrlFetcher() {}

  bool Fetch() {
    network_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SyncUrlFetcher::FetchOnIOThread,
                                  base::Unretained(this)));
    event_.Wait();
    return success_;
  }

  void FetchOnIOThread() {
    DCHECK(network_task_runner_->RunsTasksInCurrentSequence());
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url_;

    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               TRAFFIC_ANNOTATION_FOR_TESTS);
    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_, base::BindOnce(&SyncUrlFetcher::OnURLLoadComplete,
                                            base::Unretained(this)));
  }

  void OnURLLoadComplete(std::unique_ptr<std::string> response_body) {
    int response_code = -1;
    if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
      response_code = loader_->ResponseInfo()->headers->response_code();

    success_ = response_code == 200 && response_body;
    if (success_)
      *response_ = std::move(*response_body);
    loader_.reset();
    event_.Signal();
  }

 private:
  GURL url_;
  network::mojom::URLLoaderFactory* url_loader_factory_;
  const scoped_refptr<base::SequencedTaskRunner> network_task_runner_;
  std::string* response_;
  base::WaitableEvent event_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  bool success_;
};

}  // namespace

NetAddress::NetAddress() : port_(-1) {}

NetAddress::NetAddress(int port) : host_("localhost"), port_(port) {}

NetAddress::NetAddress(const std::string& host, int port)
    : host_(host), port_(port) {}

NetAddress::~NetAddress() {}

bool NetAddress::IsValid() const {
  return port_ >= 0 && port_ < (1 << 16);
}

std::string NetAddress::ToString() const {
  return host_ + base::StringPrintf(":%d", port_);
}

const std::string& NetAddress::host() const {
  return host_;
}

int NetAddress::port() const {
  return port_;
}

void SetIOCapableTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  g_io_capable_task_runner_for_tests.Get() = task_runner;
}

bool FetchUrl(const std::string& url,
              network::mojom::URLLoaderFactory* factory,
              std::string* response) {
  return SyncUrlFetcher(GURL(url), factory, response).Fetch();
}
