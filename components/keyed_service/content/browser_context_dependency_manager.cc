// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"

void BrowserContextDependencyManager::RegisterProfilePrefsForServices(
    user_prefs::PrefRegistrySyncable* pref_registry) {
  TRACE_EVENT0(
     "browser",
     "BrowserContextDependencyManager::RegisterProfilePrefsForServices");
  RegisterPrefsForServices(pref_registry);
}

void BrowserContextDependencyManager::CreateBrowserContextServices(
    content::BrowserContext* context) {
  DoCreateBrowserContextServices(context, false);
}

void BrowserContextDependencyManager::CreateBrowserContextServicesForTest(
    content::BrowserContext* context) {
  DoCreateBrowserContextServices(context, true);
}

void BrowserContextDependencyManager::DoCreateBrowserContextServices(
    content::BrowserContext* context,
    bool is_testing_context) {
  TRACE_EVENT0(
      "browser",
      "BrowserContextDependencyManager::DoCreateBrowserContextServices");
  create_services_callbacks_.Notify(context);
  DependencyManager::CreateContextServices(context, is_testing_context);
}

void BrowserContextDependencyManager::DestroyBrowserContextServices(
    content::BrowserContext* context) {
  DependencyManager::DestroyContextServices(context);
}

base::CallbackListSubscription
BrowserContextDependencyManager::RegisterCreateServicesCallbackForTesting(
    const CreateServicesCallback& callback) {
  return create_services_callbacks_.Add(callback);
}

void BrowserContextDependencyManager::AssertBrowserContextWasntDestroyed(
    content::BrowserContext* context) const {
  DependencyManager::AssertContextWasntDestroyed(context);
}

void BrowserContextDependencyManager::MarkBrowserContextLive(
    content::BrowserContext* context) {
  DependencyManager::MarkContextLive(context);
}

// static
BrowserContextDependencyManager*
BrowserContextDependencyManager::GetInstance() {
  static base::NoDestructor<BrowserContextDependencyManager> factory;
  return factory.get();
}

BrowserContextDependencyManager::BrowserContextDependencyManager() {
}

BrowserContextDependencyManager::~BrowserContextDependencyManager() {
}
