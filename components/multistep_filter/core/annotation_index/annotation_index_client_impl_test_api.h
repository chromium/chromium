// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_TEST_API_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_TEST_API_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "components/multistep_filter/core/annotation_index/annotation_index_client_impl.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace multistep_filter {

// Exposes private methods of AnnotationIndexClientImpl for testing.
class AnnotationIndexClientImplTestApi {
 public:
  explicit AnnotationIndexClientImplTestApi(AnnotationIndexClientImpl& client)
      : client_(client) {}

  void ExecuteRequest(
      std::unique_ptr<network::ResourceRequest> request,
      std::string request_body,
      base::OnceCallback<void(std::optional<std::string>)> callback) {
    client_->ExecuteRequest(std::move(request), std::move(request_body),
                            std::move(callback));
  }

 private:
  raw_ref<AnnotationIndexClientImpl> client_;
};

inline AnnotationIndexClientImplTestApi test_api(
    AnnotationIndexClientImpl& client) {
  return AnnotationIndexClientImplTestApi(client);
}

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_ANNOTATION_INDEX_ANNOTATION_INDEX_CLIENT_IMPL_TEST_API_H_
