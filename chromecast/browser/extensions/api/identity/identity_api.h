// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
#define CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_

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

  IdentityGetAuthTokenFunction(const IdentityGetAuthTokenFunction&) = delete;
  IdentityGetAuthTokenFunction& operator=(const IdentityGetAuthTokenFunction&) =
      delete;

 protected:
  ~IdentityGetAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Stub. See the IDL file for documentation.
class IdentityRemoveCachedAuthTokenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken", UNKNOWN)

  IdentityRemoveCachedAuthTokenFunction();

  IdentityRemoveCachedAuthTokenFunction(
      const IdentityRemoveCachedAuthTokenFunction&) = delete;
  IdentityRemoveCachedAuthTokenFunction& operator=(
      const IdentityRemoveCachedAuthTokenFunction&) = delete;

 protected:
  ~IdentityRemoveCachedAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace cast
}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_API_IDENTITY_IDENTITY_API_H_
