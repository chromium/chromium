// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace cast {

// Returns an OAuth2 access token for a user. See the IDL file for
// documentation.
class IdentityGetAuthTokenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.getAuthToken", UNKNOWN)

  IdentityGetAuthTokenFunction();

 protected:
  ~IdentityGetAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityGetAuthTokenFunction);
};

// Stub. See the IDL file for documentation.
class IdentityRemoveCachedAuthTokenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken", UNKNOWN)

  IdentityRemoveCachedAuthTokenFunction();

 protected:
  ~IdentityRemoveCachedAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityRemoveCachedAuthTokenFunction);
};

}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
