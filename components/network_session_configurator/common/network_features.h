// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_

#include "base/feature_list.h"
#include "network_session_configurator_export.h"

namespace features {

// Enabled DNS over HTTPS
// (https://tools.ietf.org/id/draft-ietf-doh-dns-over-https-12.txt).
NETWORK_SESSION_CONFIGURATOR_EXPORT extern const base::Feature kDnsOverHttps;

}  // namespace features

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_
