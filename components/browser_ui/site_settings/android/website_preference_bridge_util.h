// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_WEBSITE_PREFERENCE_BRIDGE_UTIL_H_
#define COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_WEBSITE_PREFERENCE_BRIDGE_UTIL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"

class ClearLocalStorageHelper : public content::BrowsingDataRemover::Observer {
 public:
  ~ClearLocalStorageHelper() override;

  static void ClearLocalStorage(content::BrowserContext* browser_context,
                                const url::Origin& origin,
                                base::OnceClosure callback);

 private:
  ClearLocalStorageHelper(content::BrowsingDataRemover* remover,
                          base::OnceClosure callback);

  // Implements content::BrowsingDataRemover::Observer.
  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override;

  raw_ptr<content::BrowsingDataRemover> remover_;
  base::OnceClosure callback_;
};

#endif  // COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_WEBSITE_PREFERENCE_BRIDGE_UTIL_H_
