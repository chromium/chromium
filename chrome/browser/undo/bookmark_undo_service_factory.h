// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class BookmarkUndoService;
class Profile;

class BookmarkUndoServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BookmarkUndoService* GetForProfile(Profile* profile);

  static BookmarkUndoService* GetForProfileIfExists(Profile* profile);

  static BookmarkUndoServiceFactory* GetInstance();

  BookmarkUndoServiceFactory(const BookmarkUndoServiceFactory&) = delete;
  BookmarkUndoServiceFactory& operator=(const BookmarkUndoServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<BookmarkUndoServiceFactory>;

  BookmarkUndoServiceFactory();
  ~BookmarkUndoServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_UNDO_BOOKMARK_UNDO_SERVICE_FACTORY_H_
