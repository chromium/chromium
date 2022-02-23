// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_

#include "chrome/browser/ui/user_education/user_education_service.h"

class FeaturePromoRegistry;
class HelpBubbleFactoryRegistry;
class TutorialRegistry;

extern const char kTabGroupTutorialId[];

extern void RegisterChromeHelpBubbleFactories(
    HelpBubbleFactoryRegistry& registry);
extern void MaybeRegisterChromeFeaturePromos(FeaturePromoRegistry& registry);
extern void MaybeRegisterChromeTutorials(TutorialRegistry& registry);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
