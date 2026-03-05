// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/mock_contextual_search_service.h"

namespace contextual_search {

MockContextualSearchService::MockContextualSearchService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    version_info::Channel channel,
    const std::string& locale)
    : ContextualSearchService(identity_manager,
                              url_loader_factory,
                              template_url_service,
                              variations_client,
                              channel,
                              locale) {}

MockContextualSearchService::~MockContextualSearchService() = default;

}  // namespace contextual_search
