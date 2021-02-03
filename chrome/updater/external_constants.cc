// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/external_constants.h"

#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/updater_branding.h"
#include "url/gurl.h"

namespace updater {

namespace {

class DefaultExternalConstants : public ExternalConstants {
 public:
  DefaultExternalConstants() : ExternalConstants(nullptr) {}
  ~DefaultExternalConstants() override = default;

  // Overrides of ExternalConstants:
  std::vector<GURL> UpdateURL() const override {
    return std::vector<GURL>{GURL(UPDATE_CHECK_URL)};
  }

  bool UseCUP() const override { return true; }

  double InitialDelay() const override { return kInitialDelay; }

  int ServerKeepAliveSeconds() const override {
    return kServerKeepAliveSeconds;
  }
};

}  // namespace

ExternalConstants::ExternalConstants(
    std::unique_ptr<ExternalConstants> next_provider)
    : next_provider_(std::move(next_provider)) {}

ExternalConstants::~ExternalConstants() = default;

std::unique_ptr<ExternalConstants> CreateExternalConstants() {
  std::unique_ptr<ExternalConstants> overrider =
      ExternalConstantsOverrider::FromDefaultJSONFile(
          std::make_unique<DefaultExternalConstants>());
  return overrider ? std::move(overrider)
                   : std::make_unique<DefaultExternalConstants>();
}

std::unique_ptr<ExternalConstants> CreateDefaultExternalConstantsForTesting() {
  return std::make_unique<DefaultExternalConstants>();
}

}  // namespace updater
