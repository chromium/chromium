// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/guest_tos_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

GuestTosScreenHandler::GuestTosScreenHandler() : BaseScreenHandler(kScreenId) {}

GuestTosScreenHandler::~GuestTosScreenHandler() = default;

void GuestTosScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("guestTosTitle", IDS_GUEST_TOS_TITLE);
  builder->Add("guestTosTermsTitle", IDS_GUEST_TOS_TERMS_TITLE);
  builder->Add("guestTosTerms", IDS_GUEST_TOS_TERMS);
  builder->Add("guestTosAccept", IDS_GUEST_TOS_ACCEPT);
  builder->Add("guestTosGoogleEulaTitle", IDS_GUEST_TOS_GOOGLE_EULA_TITLE);
  builder->Add("guestTosCrosEulaTitle", IDS_GUEST_TOS_CROS_EULA_TITLE);
  builder->Add("guestTosUsageOptinTitle", IDS_GUEST_TOS_USAGE_OPT_IN_TITLE);
  builder->Add("guestTosUsageOptin", IDS_GUEST_TOS_USAGE_OPT_IN);
  builder->Add("guestTosUsageOptInLearnMore",
               IDS_GUEST_TOS_USAGE_OPT_IN_LEARN_MORE);
  builder->Add("guestTosLearnMore", IDS_GUEST_TOS_USAGE_OPT_IN_LEARN_MORE_LINK);
  builder->Add("guestTosOk", IDS_GUEST_TOS_OK);
  builder->Add("guestTosLoading", IDS_GUEST_TOS_LOADING);
}

void GuestTosScreenHandler::Show(const std::string& google_eula_url,
                                 const std::string& cros_eula_url) {
  base::Value::Dict data;
  data.Set("googleEulaUrl", google_eula_url);
  data.Set("crosEulaUrl", cros_eula_url);
  ShowInWebUI(std::move(data));
}

base::WeakPtr<GuestTosScreenView> GuestTosScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
