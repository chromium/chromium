// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_

#include "chrome/browser/ui/user_education/user_education_service.h"

namespace user_education {
class FeaturePromoRegistry;
class HelpBubbleFactoryRegistry;
class TutorialRegistry;
class HelpBubbleDelegate;
}  // namespace user_education

extern const char kTabGroupTutorialId[];
extern const char kTabGroupWithExistingGroupTutorialId[];

extern user_education::HelpBubbleDelegate* GetHelpBubbleDelegate();
extern void RegisterChromeHelpBubbleFactories(
    user_education::HelpBubbleFactoryRegistry& registry);
extern void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);
extern void MaybeRegisterChromeTutorials(
    user_education::TutorialRegistry& registry);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
