// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SERVICE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SERVICE_H_

#include "components/contextual_search/contextual_search_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_search {

class MockContextualSearchService : public ContextualSearchService {
 public:
  MockContextualSearchService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~MockContextualSearchService() override;

  MOCK_METHOD(std::unique_ptr<ContextualSearchSessionHandle>,
              CreateSession,
              (std::unique_ptr<ContextualSearchContextController::ConfigParams>,
               ContextualSearchSource,
               std::optional<lens::LensOverlayInvocationSource>),
              (override));
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SERVICE_H_
