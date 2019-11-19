// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_
#define CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_

#include <string>

#include "chrome/services/app_service/public/mojom/types.mojom.h"

namespace apps_util {

// Create intent filter that contains only the |scheme|.
apps::mojom::IntentFilterPtr CreateSchemeOnlyFilter(const std::string& scheme);

// Create intent filter that contains only the |scheme| and |host|.
apps::mojom::IntentFilterPtr CreateSchemeAndHostOnlyFilter(
    const std::string& scheme,
    const std::string& host);

}  // namespace apps_util

#endif  // CHROME_SERVICES_APP_SERVICE_PUBLIC_CPP_INTENT_TEST_UTIL_H_
