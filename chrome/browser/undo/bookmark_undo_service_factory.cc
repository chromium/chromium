// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_service_factory.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/undo/bookmark_undo_service.h"

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
BookmarkUndoServiceFactory* BookmarkUndoServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkUndoServiceFactory> instance;
  return instance.get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : ProfileKeyedServiceFactory(
          "BookmarkUndoService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest session.
              // (Bookmarks can be enabled in Guest sessions under some
              // enterprise policies.)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not have/need access to bookmarks.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() = default;

KeyedService* BookmarkUndoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BookmarkUndoService;
}
