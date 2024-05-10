// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_LOW_USAGE_PROMO_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_LOW_USAGE_PROMO_H_

#include "components/user_education/common/feature_promo_specification.h"

class Profile;

// Creates the rotating re-engagement promotion.
//
// Some sub-promos will only be added if `profile` is specified and meets
// certain requirements.
user_education::FeaturePromoSpecification CreateLowUsagePromoSpecification(
    Profile* profile);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_LOW_USAGE_PROMO_H_
