// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_server_proxy.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/lazy_instance.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace commerce {

ClusterServerProxy::ClusterServerProxy(
    signin::IdentityManager* identity_manager,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {}

ClusterServerProxy::~ClusterServerProxy() = default;

void ClusterServerProxy::GetComparableUrls(const std::set<GURL>& product_urls,
                                           GetComparableUrlsCallback callback) {
  std::move(callback).Run(/* success= */ true, product_urls);
}

}  // namespace commerce
