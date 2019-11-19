// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/discover/modules/discover_module_redeem_offers.h"

#include "chrome/browser/ui/webui/chromeos/login/discover/discover_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

namespace {

class DiscoverModuleRedeemOffersHandler : public DiscoverHandler {
 public:
  explicit DiscoverModuleRedeemOffersHandler(
      JSCallsContainer* js_calls_container);
  ~DiscoverModuleRedeemOffersHandler() override = default;

 private:
  // BaseWebUIHandler: implementation
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;
  void RegisterMessages() override;

  DISALLOW_COPY_AND_ASSIGN(DiscoverModuleRedeemOffersHandler);
};

DiscoverModuleRedeemOffersHandler::DiscoverModuleRedeemOffersHandler(
    JSCallsContainer* js_calls_container)
    : DiscoverHandler(js_calls_container) {}

void DiscoverModuleRedeemOffersHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("discoverRedeemYourOffers", IDS_DISCOVER_REDEEM_YOUR_OFFERS);
}

void DiscoverModuleRedeemOffersHandler::Initialize() {}

void DiscoverModuleRedeemOffersHandler::RegisterMessages() {}

}  // anonymous namespace

/* ***************************************************************** */
/* Discover RedeemOffers module implementation below.                */

const char DiscoverModuleRedeemOffers::kModuleName[] = "redeem-offers";

DiscoverModuleRedeemOffers::DiscoverModuleRedeemOffers() = default;

DiscoverModuleRedeemOffers::~DiscoverModuleRedeemOffers() = default;

bool DiscoverModuleRedeemOffers::IsCompleted() const {
  return false;
}

std::unique_ptr<DiscoverHandler> DiscoverModuleRedeemOffers::CreateWebUIHandler(
    JSCallsContainer* js_calls_container) {
  return std::make_unique<DiscoverModuleRedeemOffersHandler>(
      js_calls_container);
}

}  // namespace chromeos
