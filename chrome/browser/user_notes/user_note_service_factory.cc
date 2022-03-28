// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_notes/user_note_service_factory.h"

#include <utility>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_notes/user_note_service_delegate_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/browser_context.h"

namespace user_notes {

// static
UserNoteService* UserNoteServiceFactory::GetForContext(
    content::BrowserContext* context) {
  return static_cast<UserNoteService*>(
      GetInstance()->GetServiceForBrowserContext(context,
                                                 /*create=*/true));
}

// static
UserNoteServiceFactory* UserNoteServiceFactory::GetInstance() {
  return base::Singleton<UserNoteServiceFactory>::get();
}

UserNoteServiceFactory::UserNoteServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "UserNoteService",
          BrowserContextDependencyManager::GetInstance()) {}

UserNoteServiceFactory::~UserNoteServiceFactory() = default;

KeyedService* UserNoteServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(IsUserNotesEnabled());
  return new UserNoteService(std::make_unique<UserNoteServiceDelegateImpl>(
      Profile::FromBrowserContext(context)));
}

content::BrowserContext* UserNoteServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // For now, the feature is not supported in Incognito mode.
  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  return context;
}

}  // namespace user_notes
