// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants_default.h"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/updater_branding.h"
#include "components/crx_file/crx_verifier.h"
#include "url/gurl.h"

namespace updater {
namespace {

constexpr std::optional<std::vector<uint8_t>> GetCrxPublicKeyHash() {
  if constexpr (std::string_view(CRX_PKHASH).empty()) {
    return std::nullopt;
  }
  return base::Base64Decode(CRX_PKHASH);
}

class DefaultExternalConstants : public ExternalConstants {
 public:
  DefaultExternalConstants() : ExternalConstants(nullptr) {}

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override {
    return std::vector<GURL>{GURL(UPDATE_CHECK_URL)};
  }

  GURL CrashUploadURL() const override { return GURL(CRASH_UPLOAD_URL); }

  GURL AppLogoURL() const override { return GURL(APP_LOGO_URL); }

  GURL EventLoggingURL() const override {
    return GURL(UPDATER_EVENT_LOGGING_URL);
  }

  bool UseCUP() const override { return true; }

  base::TimeDelta InitialDelay() const override { return kInitialDelay; }

  base::TimeDelta ServerKeepAliveTime() const override {
    return kServerKeepAliveTime;
  }

  crx_file::VerifierFormat CrxVerifierFormat() const override {
    return crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;
  }

  std::optional<std::vector<uint8_t>> CrxPublicKeyHash() const override {
    return GetCrxPublicKeyHash();
  }

  base::Value::Dict DictPolicies() const override {
    return base::Value::Dict();
  }

  base::TimeDelta OverinstallTimeout() const override {
    return base::Minutes(2);
  }

  base::TimeDelta IdleCheckPeriod() const override { return base::Minutes(5); }

  std::optional<bool> IsMachineManaged() const override { return std::nullopt; }

  base::TimeDelta CecaConnectionTimeout() const override {
    return kCecaConnectionTimeout;
  }

  base::TimeDelta MinimumEventLoggingCooldown() const override {
    return kMinimumEventLoggingCooldown;
  }

  std::optional<EventLoggingPermissionProvider>
  GetEventLoggingPermissionProvider() const override {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC))
    return EventLoggingPermissionProvider{
        .app_id = BROWSER_APPID,
#if BUILDFLAG(IS_MAC)
        .directory_name = BROWSER_NAME_STRING,
#endif
    };
#else
    return std::nullopt;
#endif
  }

 private:
  ~DefaultExternalConstants() override = default;
};

}  // namespace

scoped_refptr<ExternalConstants> CreateDefaultExternalConstants() {
  return base::MakeRefCounted<DefaultExternalConstants>();
}

}  // namespace updater
