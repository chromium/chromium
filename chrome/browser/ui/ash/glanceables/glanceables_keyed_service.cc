// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/glanceables/glanceables_v2_controller.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_tasks_client_impl.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace ash {

GlanceablesKeyedService::GlanceablesKeyedService(Profile* profile)
    : profile_(profile),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile_)),
      account_id_(BrowserContextHelper::Get()
                      ->GetUserByBrowserContext(profile)
                      ->GetAccountId()) {
  PrefService* const pref_service = profile->GetPrefs();

  CHECK(pref_service);

  // Listen to pref changes.
  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kGlanceablesEnabled,
      base::BindRepeating(&GlanceablesKeyedService::UpdateRegistration,
                          base::Unretained(this)));

  UpdateRegistration();
}

GlanceablesKeyedService::~GlanceablesKeyedService() = default;

void GlanceablesKeyedService::Shutdown() {
  ClearClients();
}

bool GlanceablesKeyedService::AreGlanceablesEnabled() const {
  PrefService* const prefs = profile_->GetPrefs();
  if (features::AreGlanceablesV2Enabled()) {
    return prefs->GetBoolean(prefs::kGlanceablesEnabled) ||
           base::CommandLine::ForCurrentProcess()->HasSwitch(
               ash::switches::kAshBypassGlanceablesPref);
  }

  if (features::AreGlanceablesV2EnabledForTrustedTesters()) {
    return prefs->IsManagedPreference(prefs::kGlanceablesEnabled) &&
           prefs->GetBoolean(prefs::kGlanceablesEnabled);
  }

  return false;
}

std::unique_ptr<google_apis::RequestSender>
GlanceablesKeyedService::CreateRequestSenderForClient(
    const std::vector<std::string>& scopes,
    const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) const {
  const auto url_loader_factory = profile_->GetURLLoaderFactory();
  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager_,
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory, scopes);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
              to the user on System UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), traffic_annotation_tag);
}

void GlanceablesKeyedService::RegisterClients() {
  const auto create_request_sender_callback = base::BindRepeating(
      &GlanceablesKeyedService::CreateRequestSenderForClient,
      base::Unretained(this));
  classroom_client_ = std::make_unique<GlanceablesClassroomClientImpl>(
      profile_, base::DefaultClock::GetInstance(),
      create_request_sender_callback);
  tasks_client_ = std::make_unique<GlanceablesTasksClientImpl>(
      create_request_sender_callback);

  Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
      account_id_, GlanceablesV2Controller::ClientsRegistration{
                       .classroom_client = classroom_client_.get(),
                       .tasks_client = tasks_client_.get()});
}

void GlanceablesKeyedService::ClearClients() {
  classroom_client_.reset();
  tasks_client_.reset();
  if (Shell::HasInstance()) {
    Shell::Get()->glanceables_v2_controller()->UpdateClientsRegistration(
        account_id_, GlanceablesV2Controller::ClientsRegistration{
                         .classroom_client = nullptr, .tasks_client = nullptr});
  }
}

void GlanceablesKeyedService::UpdateRegistration() {
  if (!Shell::HasInstance()) {
    return;
  }

  DCHECK(Shell::Get()->glanceables_v2_controller());

  PrefService* prefs = profile_->GetPrefs();

  CHECK(prefs);

  if (!AreGlanceablesEnabled()) {
    Shell::Get()->glanceables_v2_controller()->ClearUserStatePrefs(prefs);
    ClearClients();
    return;
  }

  if (!classroom_client_ || !tasks_client_) {
    RegisterClients();
  }
}

}  // namespace ash
