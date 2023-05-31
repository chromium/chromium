// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BookmarkUndoService;
class Profile;

class BookmarkUndoServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BookmarkUndoService* GetForProfile(Profile* profile);

  static BookmarkUndoService* GetForProfileIfExists(Profile* profile);

  static BookmarkUndoServiceFactory* GetInstance();

  BookmarkUndoServiceFactory(const BookmarkUndoServiceFactory&) = delete;
  BookmarkUndoServiceFactory& operator=(const BookmarkUndoServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<BookmarkUndoServiceFactory>;

  BookmarkUndoServiceFactory();
  ~BookmarkUndoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_
