// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_
#define CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace enterprise_companion {

// Manages a TransitionalURLLoaderFactoryOwner, providing a
// base::SequenceBound-compatible interface to create
// PendingSharedURLLoaderFactory instances on an IO thread which may be
// materialized on other threads. This class must be used on a single sequence
// with an IO message pump.
class URLLoaderFactoryProvider {
 public:
  URLLoaderFactoryProvider();

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingURLLoaderFactory();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner_;
};

}  // namespace enterprise_companion

#endif  // CHROME_ENTERPRISE_COMPANION_URL_LOADER_FACTORY_PROVIDER_H_
