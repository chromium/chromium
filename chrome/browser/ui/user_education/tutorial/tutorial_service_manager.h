// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_MANAGER_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_MANAGER_H_

#include "base/memory/singleton.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"

// TutorialServiceManager is a singleton that provides the platform specific
// factory for a TutorialService.
class TutorialServiceManager {
 public:
  TutorialServiceManager();
  ~TutorialServiceManager();
  TutorialServiceManager(const TutorialServiceManager&) = delete;
  TutorialServiceManager& operator=(const TutorialServiceManager&) = delete;

  static TutorialServiceManager* GetInstance();

  // Returns a TutorialService which is provided by the ServiceFactory.
  TutorialService* GetTutorialServiceForProfile(Profile* profile);

  // Returns the factory registry for use in the tutorial step callbacks.
  TutorialBubbleFactoryRegistry* bubble_factory_registry() {
    return bubble_factory_registry_.get();
  }

  // Getters for the registries
  TutorialRegistry* tutorial_registry() { return tutorial_registry_.get(); }

  const TutorialRegistry* tutorial_registry() const {
    return tutorial_registry_.get();
  }

 private:
  friend struct base::DefaultSingletonTraits<TutorialServiceManager>;

  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_factory_registry_;

  std::unique_ptr<TutorialRegistry> tutorial_registry_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_MANAGER_H_
