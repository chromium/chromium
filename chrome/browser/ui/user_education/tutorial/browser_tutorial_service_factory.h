// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_BROWSER_TUTORIAL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_BROWSER_TUTORIAL_SERVICE_FACTORY_H_

#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_identifier.h"

// TODO (dpenning): Move this file into views code and instantiate the factory
// from views framework code.

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

class TutorialService;

// A factory to create a unique TutorialService.
class BrowserTutorialServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TutorialService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of the Factory.
  static BrowserTutorialServiceFactory* GetInstance();

  // In some cases it is not possible to get a browser based element context
  // without going through the browser view from the profile. Since the
  // implenentations of the frameworks should not be exposed to the systems
  // building the tutorials, this method is provided to return an element
  // context.
  static ui::ElementContext GetDefaultElementContextForProfile(
      Profile* profile);

  // Disallow copy/assign.
  BrowserTutorialServiceFactory(const BrowserTutorialServiceFactory&) = delete;
  BrowserTutorialServiceFactory& operator=(
      const BrowserTutorialServiceFactory&) = delete;

  void RegisterBubbleFactories();
  void RegisterTutorials();

 private:
  friend struct base::DefaultSingletonTraits<BrowserTutorialServiceFactory>;

  BrowserTutorialServiceFactory();
  ~BrowserTutorialServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  bool registered_bubble_factories_ = false;
  bool registered_tutorials_ = false;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_BROWSER_TUTORIAL_SERVICE_FACTORY_H_
