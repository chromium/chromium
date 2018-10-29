// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/common/network_features.h"

#include "build/build_config.h"

namespace features {

const base::Feature kDnsOverHttps{"dns-over-https",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
