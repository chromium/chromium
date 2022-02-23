// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/user_education_service.h"

#include <memory>

#include "chrome/browser/ui/user_education/feature_promo_registry.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"

UserEducationService::UserEducationService()
    : tutorial_service_(&tutorial_registry_, &help_bubble_factory_registry_) {}

UserEducationService::~UserEducationService() = default;
