// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_
#define COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class CookieCryptoDelegate;
}  // namespace net

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace cookie_config {

// Factory method for returning a CookieCryptoDelegate if one is appropriate for
// this platform.
COMPONENT_EXPORT(COMPONENTS_COOKIE_CONFIG)
std::unique_ptr<net::CookieCryptoDelegate> GetCookieCryptoDelegate(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

}  // namespace cookie_config

#endif  // COMPONENTS_COOKIE_CONFIG_COOKIE_STORE_UTIL_H_
