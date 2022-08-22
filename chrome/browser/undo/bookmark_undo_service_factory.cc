// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_service_factory.h"

#include "chrome/browser/profiles/profile.h"
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
  return base::Singleton<BookmarkUndoServiceFactory>::get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : ProfileKeyedServiceFactory("BookmarkUndoService") {}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() {
}

KeyedService* BookmarkUndoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BookmarkUndoService;
}
