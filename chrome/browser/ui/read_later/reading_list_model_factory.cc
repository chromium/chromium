// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_later/reading_list_model_factory.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "components/reading_list/core/reading_list_model_storage_impl.h"
#include "components/reading_list/core/reading_list_pref_names.h"
#include "components/sync/model/model_type_store_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace {

std::unique_ptr<KeyedService> BuildReadingListModel(
    content::BrowserContext* context) {
  Profile* const profile = Profile::FromBrowserContext(context);
  syncer::OnceModelTypeStoreFactory store_factory =
      ModelTypeStoreServiceFactory::GetForProfile(profile)->GetStoreFactory();
  auto storage =
      std::make_unique<ReadingListModelStorageImpl>(std::move(store_factory));

  return std::make_unique<ReadingListModelImpl>(
      std::move(storage), base::DefaultClock::GetInstance());
}

}  // namespace

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReadingListModelImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ReadingListModelFactory* ReadingListModelFactory::GetInstance() {
  return base::Singleton<ReadingListModelFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
ReadingListModelFactory::GetDefaultFactoryForTesting() {
  return base::BindRepeating(&BuildReadingListModel);
}

ReadingListModelFactory::ReadingListModelFactory()
    : ProfileKeyedServiceFactory(
          "ReadingListModel",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
}

ReadingListModelFactory::~ReadingListModelFactory() = default;

KeyedService* ReadingListModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildReadingListModel(context).release();
}

void ReadingListModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      reading_list::prefs::kDeprecatedReadingListHasUnseenEntries, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      reading_list::prefs::kReadingListDesktopFirstUseExperienceShown, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool ReadingListModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
