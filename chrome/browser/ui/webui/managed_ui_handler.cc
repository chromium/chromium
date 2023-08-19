// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/managed_ui_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

policy::PolicyService* GetProfilePolicyService(Profile* profile) {
  auto* profile_connector = profile->GetProfilePolicyConnector();
  return profile_connector->policy_service();
}

}  // namespace

// static
void ManagedUIHandler::Initialize(content::WebUI* web_ui,
                                  content::WebUIDataSource* source) {
  InitializeInternal(web_ui, source, Profile::FromWebUI(web_ui));
}

// static
void ManagedUIHandler::InitializeInternal(content::WebUI* web_ui,
                                          content::WebUIDataSource* source,
                                          Profile* profile) {
  auto handler = std::make_unique<ManagedUIHandler>(profile);
  source->AddLocalizedStrings(handler->GetDataSourceUpdate());
  handler->source_name_ = source->GetSource();
  web_ui->AddMessageHandler(std::move(handler));
}

ManagedUIHandler::ManagedUIHandler(Profile* profile)
    : profile_(profile), managed_(chrome::ShouldDisplayManagedUi(profile_)) {
  pref_registrar_.Init(profile_->GetPrefs());
}

ManagedUIHandler::~ManagedUIHandler() {
  RemoveObservers();
}

void ManagedUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "observeManagedUI",
      base::BindRepeating(&ManagedUIHandler::HandleObserveManagedUI,
                          base::Unretained(this)));
}

void ManagedUIHandler::HandleObserveManagedUI(
    const base::Value::List& /*args*/) {
  AllowJavascript();
  AddObservers();
}

void ManagedUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagedUIHandler::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                       const policy::PolicyMap& previous,
                                       const policy::PolicyMap& current) {
  NotifyIfChanged();
}

// Manually add/remove observers. ScopedObserver doesn't work with
// PolicyService::Observer because AddObserver() takes 2 arguments.
void ManagedUIHandler::AddObservers() {
  if (has_observers_)
    return;

  has_observers_ = true;

  auto* policy_service = GetProfilePolicyService(profile_);
  for (int i = 0; i < policy::POLICY_DOMAIN_SIZE; i++) {
    auto domain = static_cast<policy::PolicyDomain>(i);
    policy_service->AddObserver(domain, this);
  }

  pref_registrar_.Add(prefs::kSupervisedUserId,
                      base::BindRepeating(&ManagedUIHandler::NotifyIfChanged,
                                          base::Unretained(this)));
}

void ManagedUIHandler::RemoveObservers() {
  if (!has_observers_)
    return;

  has_observers_ = false;

  auto* policy_service = GetProfilePolicyService(profile_);
  for (int i = 0; i < policy::POLICY_DOMAIN_SIZE; i++) {
    auto domain = static_cast<policy::PolicyDomain>(i);
    policy_service->RemoveObserver(domain, this);
  }

  pref_registrar_.RemoveAll();
}

base::Value::Dict ManagedUIHandler::GetDataSourceUpdate() const {
  base::Value::Dict update;
  update.Set("managedByIcon", chrome::GetManagedUiWebUIIcon(profile_));
  update.Set("managementPageUrl", chrome::GetManagedUiUrl(profile_).spec());
  update.Set("browserManagedByOrg", chrome::GetManagedUiWebUILabel(profile_));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  update.Set("deviceManagedByOrg", chrome::GetDeviceManagedUiWebUILabel());
#endif
  update.Set("isManaged", managed_);
  return update;
}

void ManagedUIHandler::NotifyIfChanged() {
  bool managed = chrome::ShouldDisplayManagedUi(profile_);
  if (managed == managed_)
    return;
  managed_ = managed;
  FireWebUIListener("is-managed-changed", base::Value(managed));
  content::WebUIDataSource::Update(profile_, source_name_,
                                   GetDataSourceUpdate());
}
