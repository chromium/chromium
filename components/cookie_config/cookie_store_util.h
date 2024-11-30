// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_
#define COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_

#include <memory>

#include "base/component_export.h"

namespace net {
class CookieCryptoDelegate;
}  // namespace net

namespace cookie_config {

// Factory method for returning a CookieCryptoDelegate if one is appropriate for
// this platform.
COMPONENT_EXPORT(COMPONENTS_COOKIE_CONFIG)
std::unique_ptr<net::CookieCryptoDelegate> GetCookieCryptoDelegate();

}  // namespace cookie_config

#endif  // COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_
