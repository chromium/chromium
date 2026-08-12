// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_PROVIDER_H_

class AccountId;
class TemplateURLService;

namespace ash {

// Provides the TemplateURLService associated with a user to ChromeOS callers
// without forcing them to depend on //chrome/browser/search_engines' Profile-
// keyed factory. The concrete implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// template_url_service_provider_impl.h).
class TemplateURLServiceProvider {
 public:
  TemplateURLServiceProvider();
  TemplateURLServiceProvider(const TemplateURLServiceProvider&) = delete;
  TemplateURLServiceProvider& operator=(const TemplateURLServiceProvider&) =
      delete;
  virtual ~TemplateURLServiceProvider();

  // Returns the process-wide singleton.
  static TemplateURLServiceProvider& Get();

  // Returns the TemplateURLService associated with `account_id`, or nullptr if
  // none is available. The returned pointer is owned by the BrowserContext-
  // keyed service infrastructure; callers must not delete it.
  virtual TemplateURLService* Find(const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_PROVIDER_H_
