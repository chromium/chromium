// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"

namespace user_education {
class FeaturePromoControllerCommon;
class FeaturePromoRegistry;
class HelpBubbleDelegate;
class HelpBubbleFactoryRegistry;
class NewBadgeRegistry;
class TutorialRegistry;
}  // namespace user_education

class BrowserView;

// These do low-level initialization of data structures required for user
// education; most code should not call them directly.
extern user_education::HelpBubbleDelegate* GetHelpBubbleDelegate();
extern void RegisterChromeHelpBubbleFactories(
    user_education::HelpBubbleFactoryRegistry& registry);
extern void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);
extern void MaybeRegisterChromeNewBadges(
    user_education::NewBadgeRegistry& registry);
extern void MaybeRegisterChromeTutorials(
    user_education::TutorialRegistry& registry);

// Creates (or doesn't create) a FeaturePromoController for the specified
// `browser_view`. Not all browser windows can do promos; specifically,
// headless, kiosk, guest, incognito, and other off-the-record browsers do
// _not_ show IPH. Initializes all other User Education data associated with the
// browser as well.
extern std::unique_ptr<user_education::FeaturePromoControllerCommon>
CreateUserEducationResources(BrowserView* browser_view);

// Adds (or doesn't add) high priority notices (usually legal and privacy
// related) to the product messaging queue for the specified `profile`. The
// order of showing is defined by the show_after_ and blocked_by_ lists when
// each notice is queued. These lists are often defined within services used in
// this method. Notices are queued in this frame and the queue begins processing
// in the next frame.
extern void QueueLegalAndPrivacyNotices(Profile* profile);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
