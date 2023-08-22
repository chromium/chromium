// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename>
class NoDestructor;
}  // namespace base

namespace user_notes {

class UserNoteService;

// Factory to get or create a UserNoteService instance for the current browser
// context.
class UserNoteServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static UserNoteService* GetForContext(content::BrowserContext* context);

  // Allows tests to set a mock UserNoteService that is going to be returned
  // by `GetForContext` every time, even if `context` is null.
  static void SetServiceForTesting(UserNoteService* service);

  static void EnsureFactoryBuilt();

  UserNoteServiceFactory(const UserNoteServiceFactory&) = delete;
  UserNoteServiceFactory& operator=(const UserNoteServiceFactory&) = delete;

 private:
  friend base::NoDestructor<UserNoteServiceFactory>;

  static UserNoteServiceFactory* GetInstance();

  UserNoteServiceFactory();
  ~UserNoteServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  raw_ptr<UserNoteService> service_for_testing_;
};

}  // namespace user_notes

#endif  // CHROME_BROWSER_USER_NOTES_USER_NOTE_SERVICE_FACTORY_H_
