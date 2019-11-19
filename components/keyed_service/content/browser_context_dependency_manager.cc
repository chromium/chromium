// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/content/browser_context_dependency_manager.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"

#ifndef NDEBUG
#include "base/command_line.h"
#include "base/files/file_util.h"

// Dumps dependency information about our browser context keyed services
// into a dot file in the browser context directory.
const char kDumpBrowserContextDependencyGraphFlag[] =
    "dump-browser-context-graph";
#endif  // NDEBUG

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
      "BrowserContextDependencyManager::DoCreateBrowserContextServices")
  will_create_browser_context_services_callbacks_.Notify(context);
  DependencyManager::CreateContextServices(context, is_testing_context);
}

void BrowserContextDependencyManager::DestroyBrowserContextServices(
    content::BrowserContext* context) {
  DependencyManager::DestroyContextServices(context);
}

std::unique_ptr<
    base::CallbackList<void(content::BrowserContext*)>::Subscription>
BrowserContextDependencyManager::
    RegisterWillCreateBrowserContextServicesCallbackForTesting(
        const base::Callback<void(content::BrowserContext*)>& callback) {
  return will_create_browser_context_services_callbacks_.Add(callback);
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

#ifndef NDEBUG
void BrowserContextDependencyManager::DumpContextDependencies(
    void* context) const {
  // Whenever we try to build a destruction ordering, we should also dump a
  // dependency graph to "/path/to/context/context-dependencies.dot".
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDumpBrowserContextDependencyGraphFlag)) {
    base::FilePath dot_file =
        static_cast<content::BrowserContext*>(context)->GetPath().AppendASCII(
            "browser-context-dependencies.dot");
    DumpDependenciesAsGraphviz("BrowserContext", dot_file);
  }
}
#endif  // NDEBUG
