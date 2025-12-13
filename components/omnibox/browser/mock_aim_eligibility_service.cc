// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/mock_aim_eligibility_service.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"

MockAimEligibilityService::MockAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    bool is_off_the_record)
    : AimEligibilityService(pref_service,
                            template_url_service,
                            url_loader_factory,
                            identity_manager,
                            is_off_the_record) {}

MockAimEligibilityService::~MockAimEligibilityService() = default;
