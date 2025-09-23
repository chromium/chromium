// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_

#include <memory>

namespace user_education {
class FeaturePromoControllerCommon;
class FeaturePromoRegistry;
class HelpBubbleDelegate;
class HelpBubbleFactoryRegistry;
class NewBadgeRegistry;
class TutorialRegistry;
}  // namespace user_education

class Profile;
class UserEducationService;

// These do low-level initialization of data structures required for user
// education; most code should not call them directly.
user_education::HelpBubbleDelegate* GetHelpBubbleDelegate();
void RegisterChromeHelpBubbleFactories(
    user_education::HelpBubbleFactoryRegistry& registry);
void MaybeRegisterChromeFeaturePromos(
    user_education::FeaturePromoRegistry& registry);
void MaybeRegisterChromeNewBadges(user_education::NewBadgeRegistry& registry);
void MaybeRegisterChromeTutorials(user_education::TutorialRegistry& registry);

// Creates (or doesn't create) a FeaturePromoController for the specified
// `service`. Not all browser windows can do promos; specifically,
// headless, kiosk, guest, incognito, and other off-the-record browsers do
// _not_ show IPH. Initializes all other User Education data associated with the
// browser as well.
std::unique_ptr<user_education::FeaturePromoControllerCommon>
CreateUserEducationResources(UserEducationService& user_education_service);

// Adds (or doesn't add) high priority notices (usually legal and privacy
// related) to the product messaging queue for the specified `profile`. The
// order of showing is defined by the show_after_ and blocked_by_ lists when
// each notice is queued. These lists are often defined within services used in
// this method. Notices are queued in this frame and the queue begins processing
// in the next frame.
void QueueLegalAndPrivacyNotices(Profile* profile);

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_BROWSER_USER_EDUCATION_SERVICE_H_
