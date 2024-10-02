// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_default.h"

#include <optional>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_branding.h"
#include "components/crx_file/crx_verifier.h"
#include "url/gurl.h"

namespace updater {
namespace {

class DefaultExternalConstants : public ExternalConstants {
 public:
  DefaultExternalConstants() : ExternalConstants(nullptr) {}

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override {
    return std::vector<GURL>{GURL(UPDATE_CHECK_URL)};
  }

  GURL CrashUploadURL() const override { return GURL(CRASH_UPLOAD_URL); }

  GURL DeviceManagementURL() const override {
    return GURL(DEVICE_MANAGEMENT_SERVER_URL);
  }

  GURL AppLogoURL() const override { return GURL(APP_LOGO_URL); }

  bool UseCUP() const override { return true; }

  base::TimeDelta InitialDelay() const override { return kInitialDelay; }

  base::TimeDelta ServerKeepAliveTime() const override {
    return kServerKeepAliveTime;
  }

  crx_file::VerifierFormat CrxVerifierFormat() const override {
    return crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  }

  base::Value::Dict GroupPolicies() const override {
    return base::Value::Dict();
  }

  base::TimeDelta OverinstallTimeout() const override {
    return base::Minutes(2);
  }

  base::TimeDelta IdleCheckPeriod() const override { return base::Minutes(5); }

  std::optional<bool> IsMachineManaged() const override { return std::nullopt; }

  bool EnableDiffUpdates() const override { return false; }

  base::TimeDelta CecaConnectionTimeout() const override {
    return kCecaConnectionTimeout;
  }

 private:
  ~DefaultExternalConstants() override = default;
};

}  // namespace

scoped_refptr<ExternalConstants> CreateDefaultExternalConstants() {
  return base::MakeRefCounted<DefaultExternalConstants>();
}

}  // namespace updater
