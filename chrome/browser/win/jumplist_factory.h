// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_
#define CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class JumpList;

class JumpListFactory : public ProfileKeyedServiceFactory {
 public:
  static JumpList* GetForProfile(Profile* profile);

  static JumpListFactory* GetInstance();

 private:
  friend base::NoDestructor<JumpListFactory>;
  JumpListFactory();
  ~JumpListFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_
