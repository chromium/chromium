// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/core/browser/password_change_success_tracker_impl.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"

namespace password_manager {

PasswordChangeSuccessTrackerFactory::PasswordChangeSuccessTrackerFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordChangeSuccessTracker",
          BrowserContextDependencyManager::GetInstance()) {}

PasswordChangeSuccessTrackerFactory::~PasswordChangeSuccessTrackerFactory() =
    default;

// static
PasswordChangeSuccessTrackerFactory*
PasswordChangeSuccessTrackerFactory::GetInstance() {
  static base::NoDestructor<PasswordChangeSuccessTrackerFactory> instance;
  return instance.get();
}

// static
PasswordChangeSuccessTracker*
PasswordChangeSuccessTrackerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PasswordChangeSuccessTracker*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

KeyedService* PasswordChangeSuccessTrackerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  auto* tracker = new PasswordChangeSuccessTrackerImpl(
      user_prefs::UserPrefs::Get(browser_context));
  tracker->AddMetricsRecorder(
      std::make_unique<PasswordChangeMetricsRecorderUma>());
  tracker->AddMetricsRecorder(
      std::make_unique<PasswordChangeMetricsRecorderUkm>());
  return tracker;
}

}  // namespace password_manager
