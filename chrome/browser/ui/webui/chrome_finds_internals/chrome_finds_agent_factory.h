// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_FACTORY_H_
#define CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chrome_finds_internals {

class ChromeFindsAgent;

class ChromeFindsAgentFactory : public ProfileKeyedServiceFactory {
 public:
  static ChromeFindsAgent* GetForProfile(Profile* profile);
  static ChromeFindsAgentFactory* GetInstance();

  ChromeFindsAgentFactory(const ChromeFindsAgentFactory&) = delete;
  ChromeFindsAgentFactory& operator=(const ChromeFindsAgentFactory&) = delete;

 private:
  friend base::NoDestructor<ChromeFindsAgentFactory>;

  ChromeFindsAgentFactory();
  ~ChromeFindsAgentFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace chrome_finds_internals

#endif  // CHROME_BROWSER_UI_WEBUI_CHROME_FINDS_INTERNALS_CHROME_FINDS_AGENT_FACTORY_H_
