// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/connectivity_checker.h"

#include "base/single_thread_task_runner.h"
#include "chromecast/net/connectivity_checker_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromecast {

ConnectivityChecker::ConnectivityChecker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : RefCountedDeleteOnSequence(std::move(task_runner)),
      connectivity_observer_list_(
          base::MakeRefCounted<
              base::ObserverListThreadSafe<ConnectivityObserver>>()) {}

ConnectivityChecker::~ConnectivityChecker() {
}

void ConnectivityChecker::AddConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->AddObserver(observer);
}

void ConnectivityChecker::RemoveConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_->RemoveObserver(observer);
}

void ConnectivityChecker::Notify(bool connected) {
  DCHECK(connectivity_observer_list_.get());
  connectivity_observer_list_->Notify(
      FROM_HERE, &ConnectivityObserver::OnConnectivityChanged, connected);
}

// static
scoped_refptr<ConnectivityChecker> ConnectivityChecker::Create(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker) {
  return ConnectivityCheckerImpl::Create(task_runner,
                                         std::move(url_loader_factory_info),
                                         network_connection_tracker);
}

}  // namespace chromecast
