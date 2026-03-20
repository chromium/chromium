// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace accessibility_annotator {

// Implementation of OnePResolver.
// Note: Currently, this class only handles fetching accessibility annotations
// for the feature from the OneP service. The next phase will resolve the query
// and the annotations into meaningful memory search results using an
// optimization keyed service.
class OnePResolverImpl : public OnePResolver {
 public:
  explicit OnePResolverImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  OnePResolverImpl(const OnePResolverImpl&) = delete;
  OnePResolverImpl& operator=(const OnePResolverImpl&) = delete;
  ~OnePResolverImpl() override;

  // OnePResolver:
  void Query(std::u16string query, QueryCallback callback) override;

 private:
  void OnAccessTokenFetched(std::u16string query,
                            GURL url,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  void OnUrlLoadComplete(std::optional<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  // These are created and destroyed during the async Query() process.
  // They are destroyed early if a new Query() arrives before the previous
  // one completes.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;

  // Callback for the currently active query request.
  QueryCallback in_flight_query_callback_;

  base::WeakPtrFactory<OnePResolverImpl> weak_ptr_factory_{this};
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_ONE_P_RESOLVER_IMPL_H_
