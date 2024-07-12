// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_IDENTITY_VALIDATOR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_IDENTITY_VALIDATOR_H_

#include "base/types/expected.h"
#include "components/web_package/signed_web_bundles/identity_validator.h"

namespace web_app {

class IwaIdentityValidator : public web_package::IdentityValidator {
 public:
  // Creates a global singleton that can be accessed via
  // `web_package::IdentityValidator::GetInstance()`.
  static void CreateSingleton();

  // web_package::IdentityValidator:
  base::expected<void, std::string> ValidateWebBundleIdentity(
      const std::string& web_bundle_id,
      const std::vector<web_package::PublicKey>& public_keys) const override;

 private:
  IwaIdentityValidator() = default;
  ~IwaIdentityValidator() override = default;

  friend base::NoDestructor<IwaIdentityValidator>;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_IWA_IDENTITY_VALIDATOR_H_
