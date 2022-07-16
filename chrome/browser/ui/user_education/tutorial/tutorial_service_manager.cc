// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_service_manager.h"

#include "build/build_config.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"

// TODO: as more platforms are tested for reliability we will add them here.
#if defined(OS_MAC) || defined(OS_WIN)
#include "chrome/browser/ui/user_education/tutorial/browser_tutorial_service_factory.h"
#endif

TutorialServiceManager::TutorialServiceManager()
    : bubble_factory_registry_(
          std::make_unique<TutorialBubbleFactoryRegistry>()),
      tutorial_registry_(std::make_unique<TutorialRegistry>()) {}
TutorialServiceManager::~TutorialServiceManager() = default;

TutorialServiceManager* TutorialServiceManager::GetInstance() {
  return base::Singleton<TutorialServiceManager>::get();
}

TutorialService* TutorialServiceManager::GetTutorialServiceForProfile(
    Profile* profile) {
#if defined(OS_MAC) || defined(OS_WIN)
  return BrowserTutorialServiceFactory::GetForProfile(profile);
#endif
#if !defined(OS_MAC) && !defined(OS_WIN)
  return nullptr;
#endif
}
