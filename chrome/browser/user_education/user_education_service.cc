// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_education/user_education_service.h"

#include <memory>
#include "components/user_education/common/feature_promo_storage_service.h"

const char kSidePanelCustomizeChromeTutorialId[] =
    "Side Panel Customize Chrome Tutorial";
const char kTabGroupTutorialId[] = "Tab Group Tutorial";
const char kPasswordManagerTutorialId[] = "Password Manager Tutorial";

UserEducationService::UserEducationService(
    std::unique_ptr<user_education::FeaturePromoStorageService> storage_service)
    : tutorial_service_(&tutorial_registry_, &help_bubble_factory_registry_),
      feature_promo_storage_service_(std::move(storage_service)) {}

UserEducationService::~UserEducationService() = default;
