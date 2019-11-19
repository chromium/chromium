// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"

class GURL;

namespace base {
class Value;
}

namespace apps {

// The preferred apps set by the user. The preferred apps is stored as
// base::DictionaryValue. It is a nested map that maps the intent filter
// condition type and value to app id. For example, to represent a
// preferred app for an intent filter that handles https://www.google.com,
// and a preferred app for an intent filter that handles tel:// link,
// the preferred_apps dictionary will look like:
// {“scheme”: {
//    "https”: {
//      “host”: {
//        “www.google.com”: {
//          "app_id": <app_id>
//        },
//      },
//    },
//    "tel": {
//      "app_id": <app_id>
//    },
//  },
// }
class PreferredApps {
 public:
  PreferredApps();
  ~PreferredApps();

  static bool VerifyPreferredApps(base::Value* dict);

  // Add a preferred app for an |intent_filter| for |preferred_apps|.
  static bool AddPreferredApp(const std::string& app_id,
                              const apps::mojom::IntentFilterPtr& intent_filter,
                              base::Value* preferred_apps);

  // Delete a preferred app for an |intent_filter| for |preferred_apps|.
  static bool DeletePreferredApp(
      const std::string& app_id,
      const apps::mojom::IntentFilterPtr& intent_filter,
      base::Value* preferred_apps);

  // Delete all settings for an |app_id|.
  static void DeleteAppId(const std::string& app_id,
                          base::Value* preferred_apps);

  void Init(std::unique_ptr<base::Value> preferred_apps);

  // Add a preferred app for an |intent_filter|.
  bool AddPreferredApp(const std::string& app_id,
                       const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete a preferred app for an |intent_filter|.
  bool DeletePreferredApp(const std::string& app_id,
                          const apps::mojom::IntentFilterPtr& intent_filter);

  // Delete all settings for an |app_id|.
  void DeleteAppId(const std::string& app_id);

  // Find preferred app id for an |intent|.
  base::Optional<std::string> FindPreferredAppForIntent(
      const apps::mojom::IntentPtr& intent);

  // Find preferred app id for an |url|.
  base::Optional<std::string> FindPreferredAppForUrl(const GURL& url);

  // Get a copy of the preferred apps.
  base::Value GetValue();

  bool IsInitialized();

 private:
  std::unique_ptr<base::Value> preferred_apps_;

  DISALLOW_COPY_AND_ASSIGN(PreferredApps);
};

}  // namespace apps

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_PREFERRED_APPS_H_
