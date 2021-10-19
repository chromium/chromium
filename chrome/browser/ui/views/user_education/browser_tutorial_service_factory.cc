// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/browser_tutorial_service_factory.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service_manager.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/user_education/tutorial_bubble_factory_views.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/interaction/element_tracker_views.h"

// static
BrowserTutorialServiceFactory* BrowserTutorialServiceFactory::GetInstance() {
  BrowserTutorialServiceFactory* factory =
      base::Singleton<BrowserTutorialServiceFactory>::get();
  factory->RegisterBubbleFactories();
  return factory;
}

// static
TutorialService* BrowserTutorialServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<TutorialService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BrowserTutorialServiceFactory::BrowserTutorialServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TutorialService",
          BrowserContextDependencyManager::GetInstance()) {}
BrowserTutorialServiceFactory::~BrowserTutorialServiceFactory() = default;

KeyedService* BrowserTutorialServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  std::unique_ptr<TutorialRegistry> tutorial_registry =
      std::make_unique<BrowserTutorialRegistry>();
  tutorial_registry->RegisterTutorials();

  std::unique_ptr<TutorialBubbleFactoryRegistry> bubble_owner_registry =
      std::make_unique<TutorialBubbleFactoryRegistry>();

  TutorialService* service = new TutorialService(std::move(tutorial_registry));

  return service;
}

void BrowserTutorialServiceFactory::RegisterBubbleFactories() {
  if (registered_bubble_factories_)
    return;

  TutorialServiceManager::GetInstance()
      ->bubble_factory_registry()
      ->RegisterBubbleFactory(std::make_unique<TutorialBubbleFactoryViews>());

  registered_bubble_factories_ = true;
}

// static
ui::ElementContext
BrowserTutorialServiceFactory::GetDefaultElementContextForProfile(
    Profile* profile) {
  Browser* const browser = chrome::FindBrowserWithProfile(profile);
  DCHECK(browser);
  if (!browser)
    return ui::ElementContext();

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  DCHECK(browser_view);
  if (!browser_view)
    return ui::ElementContext();
  return views::ElementTrackerViews::GetContextForView(browser_view);
}

void BrowserTutorialRegistry::RegisterTutorials() {
  {
    TutorialDescription* description = new TutorialDescription();
    TutorialDescription::Step step1(
        absl::nullopt,
        u"Right Click on a Tab and select \"Add Tab To new Group\".",
        ui::InteractionSequence::StepType::kShown,
        TabStrip::kTabStripIdentifier, TutorialDescription::Step::Arrow::TOP);
    description->steps.emplace_back(step1);

    TutorialDescription::Step step2(
        absl::nullopt, u"Select \"Enter a name for your Tab Group\".",
        ui::InteractionSequence::StepType::kShown,
        TabGroupEditorBubbleView::kEditorBubbleIdentifier,
        TutorialDescription::Step::Arrow::CENTER_HORIZONTAL);
    description->steps.emplace_back(step2);

    AddTutorial("Tab Group Tutorial", *description);
  }
}
