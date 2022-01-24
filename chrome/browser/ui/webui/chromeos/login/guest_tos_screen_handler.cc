// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/guest_tos_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/guest_tos_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId GuestTosScreenView::kScreenId;

GuestTosScreenHandler::GuestTosScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.GuestTosScreen.userActed");
}

GuestTosScreenHandler::~GuestTosScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

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
  builder->Add("guestTosLearnMore", IDS_GUEST_TOS_LEARN_MORE);
  builder->Add("guestTosOk", IDS_GUEST_TOS_OK);
  builder->Add("guestTosLoading", IDS_GUEST_TOS_LOADING);
}

void GuestTosScreenHandler::Initialize() {
  if (show_on_init_) {
    Show(google_eula_url_, cros_eula_url_);
    show_on_init_ = false;
  }
}

void GuestTosScreenHandler::Show(const std::string& google_eula_url,
                                 const std::string& cros_eula_url) {
  google_eula_url_ = google_eula_url;
  cros_eula_url_ = cros_eula_url;
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  base::DictionaryValue data;
  data.SetString("googleEulaUrl", google_eula_url);
  data.SetString("crosEulaUrl", cros_eula_url);
  ShowScreenWithData(kScreenId, &data);
}

void GuestTosScreenHandler::Bind(GuestTosScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void GuestTosScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void GuestTosScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();
  AddCallback("GuestToSAccept", &GuestTosScreenHandler::HandleAccept);
}

void GuestTosScreenHandler::HandleAccept(bool enable_usage_stats) {
  if (screen_)
    screen_->OnAccept(enable_usage_stats);
}
}  // namespace chromeos
