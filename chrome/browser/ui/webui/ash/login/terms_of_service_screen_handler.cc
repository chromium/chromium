// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/terms_of_service_screen_handler.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace ash {

TermsOfServiceScreenHandler::TermsOfServiceScreenHandler()
    : BaseScreenHandler(kScreenId) {}

TermsOfServiceScreenHandler::~TermsOfServiceScreenHandler() = default;

void TermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("termsOfServiceScreenHeading",
               IDS_TERMS_OF_SERVICE_SCREEN_HEADING);
  builder->Add("termsOfServiceScreenSubheading",
               IDS_TERMS_OF_SERVICE_SCREEN_SUBHEADING);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
  builder->Add("termsOfServiceError", IDS_TERMS_OF_SERVICE_SCREEN_ERROR);
  builder->Add("termsOfServiceTryAgain", IDS_TERMS_OF_SERVICE_SCREEN_TRY_AGAIN);
  builder->Add("termsOfServiceBackButton",
               IDS_TERMS_OF_SERVICE_SCREEN_BACK_BUTTON);
  builder->Add("termsOfServiceAcceptButton",
               IDS_TERMS_OF_SERVICE_SCREEN_ACCEPT_BUTTON);
  builder->Add("termsOfServiceRetryButton",
               IDS_TERMS_OF_SERVICE_SCREEN_RETRY_BUTTON);
}

void TermsOfServiceScreenHandler::Show(const std::string& manager) {
  base::Value::Dict data;
  data.Set("manager", manager);

  ShowInWebUI(std::move(data));
}

void TermsOfServiceScreenHandler::OnLoadError() {
  terms_loaded_ = false;
  CallExternalAPI("setTermsOfServiceLoadError");
}

void TermsOfServiceScreenHandler::OnLoadSuccess(
    const std::string& terms_of_service) {
  terms_loaded_ = true;
  CallExternalAPI("setTermsOfService", terms_of_service);
}

bool TermsOfServiceScreenHandler::AreTermsLoaded() {
  return terms_loaded_;
}

base::WeakPtr<TermsOfServiceScreenView>
TermsOfServiceScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
