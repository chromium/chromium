// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "chrome/browser/ui/cookie_controls/cookie_controls_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

namespace {
static const char* kPolicyIcon = "cr20:domain";
static const char* kExtensionIcon = "cr:extension";
static const char* kSettingsIcon = "cr:settings_icon";
}  // namespace

CookieControlsHandler::CookieControlsHandler(Profile* profile)
    : service_(CookieControlsServiceFactory::GetForProfile(profile)) {}

CookieControlsHandler::~CookieControlsHandler() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cookieControlsToggleChanged",
      base::BindRepeating(
          &CookieControlsHandler::HandleCookieControlsToggleChanged,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeCookieControlsSettingsChanges",
      base::BindRepeating(
          &CookieControlsHandler::HandleObserveCookieControlsSettingsChanges,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptAllowed() {
  service_->AddObserver(this);
}

void CookieControlsHandler::OnJavascriptDisallowed() {
  service_->RemoveObserver(this);
}

void CookieControlsHandler::HandleCookieControlsToggleChanged(
    const base::Value::List& args) {
  CHECK(!args.empty());
  const bool checked = args[0].GetBool();
  service_->HandleCookieControlsToggleChanged(checked);
}

void CookieControlsHandler::HandleObserveCookieControlsSettingsChanges(
    const base::Value::List& args) {
  AllowJavascript();
  SendCookieControlsUIChanges();
}

const char* CookieControlsHandler::GetEnforcementIcon(
    CookieControlsEnforcement enforcement) {
  switch (enforcement) {
    case CookieControlsEnforcement::kEnforcedByPolicy:
      return kPolicyIcon;
    case CookieControlsEnforcement::kEnforcedByExtension:
      return kExtensionIcon;
    case CookieControlsEnforcement::kEnforcedByCookieSetting:
      return kSettingsIcon;
    case CookieControlsEnforcement::kEnforcedByTpcdGrant:
    case CookieControlsEnforcement::kNoEnforcement:
      return "";
  }
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPrefChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPolicyChanged() {
  SendCookieControlsUIChanges();
}

void CookieControlsHandler::SendCookieControlsUIChanges() {
  base::Value::Dict dict;
  dict.Set("enforced", service_->ShouldEnforceCookieControls());
  dict.Set("checked", service_->GetToggleCheckedValue());
  dict.Set("icon",
           GetEnforcementIcon(service_->GetCookieControlsEnforcement()));
  dict.Set("cookieSettingsUrl", chrome::kChromeUICookieSettingsURL);
  FireWebUIListener("cookie-controls-changed", dict);
}
