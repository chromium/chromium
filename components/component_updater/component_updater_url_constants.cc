// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_url_constants.h"

namespace component_updater {

// The default URL for the v3 protocol service endpoint. In some cases, the
// component updater is allowed to fall back to other URL endpoints, if
// the request to the default URL source fails.
//
// The responses to the requests made to these endpoints are always signed.
//
// The value of |kDefaultUrlSource| can be overridden with
// --component-updater=url-source=someurl.
const char kUpdaterJSONDefaultUrl[] =
    "https://update.googleapis.com/service/update2/json";

const char kUpdaterJSONFallbackUrl[] =
    "http://update.googleapis.com/service/update2/json";

}  // namespace component_updater
