// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/prefs/pref_service.h"
#include "ui/events/event_constants.h"

namespace arc {

namespace {

constexpr base::TimeDelta kMinRetryTime = base::TimeDelta::FromMinutes(2);
constexpr base::TimeDelta kMaxRetryTime = base::TimeDelta::FromMinutes(30);

}  // namespace

ArcPaiStarter::ArcPaiStarter(Profile* profile)
    : profile_(profile),
      pref_service_(profile->GetPrefs()),
      retry_interval_(kMinRetryTime) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  // Prefs may not available in some unit tests.
  if (!prefs)
    return;
  prefs->AddObserver(this);
  MaybeStartPai();
}

ArcPaiStarter::~ArcPaiStarter() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs)
    return;
  prefs->RemoveObserver(this);
}

// static
std::unique_ptr<ArcPaiStarter> ArcPaiStarter::CreateIfNeeded(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kArcPaiStarted))
    return std::unique_ptr<ArcPaiStarter>();
  return std::make_unique<ArcPaiStarter>(profile);
}

void ArcPaiStarter::AcquireLock() {
  DCHECK(!locked_);
  locked_ = true;
}

void ArcPaiStarter::ReleaseLock() {
  DCHECK(locked_);
  locked_ = false;
  MaybeStartPai();
}

void ArcPaiStarter::AddOnStartCallback(base::OnceClosure callback) {
  if (started_) {
    std::move(callback).Run();
    return;
  }

  onstart_callbacks_.push_back(std::move(callback));
}

void ArcPaiStarter::TriggerRetryForTesting() {
  retry_timer_.FireNow();
}

void ArcPaiStarter::MaybeStartPai() {
  // Clear retry timer. It is only used to call |MaybeStartPai| in case of PAI
  // flow failed and no condition is changed to trigger |MaybeStartPai|.
  retry_timer_.Stop();

  if (started_ || pending_ || locked_ || IsArcPlayAutoInstallDisabled())
    return;

  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(kPlayStoreAppId);
  if (!app_info || !app_info->ready)
    return;

  arc::mojom::AppInstance* app_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc::ArcServiceManager::Get()->arc_bridge_service()->app(), StartPaiFlow);

  if (!app_instance) {
    app_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
        StartPaiFlowDeprecated);
    // this should always be set because PAI can be started only in case Play
    // Store app is ready which means app_instance is connected.
    DCHECK(app_instance);
    VLOG(1) << "Start deprecated PAI flow";
    app_instance->StartPaiFlowDeprecated();
    OnPaiDone();
    return;
  }

  VLOG(1) << "Start PAI flow";
  pending_ = true;
  request_start_time_ = base::Time::Now();
  app_instance->StartPaiFlow(base::BindOnce(&ArcPaiStarter::OnPaiRequested,
                                            weak_ptr_factory_.GetWeakPtr()));
}

void ArcPaiStarter::OnPaiDone() {
  DCHECK(!pending_);
  ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(prefs);

  started_ = true;
  pref_service_->SetBoolean(prefs::kArcPaiStarted, true);

  prefs->RemoveObserver(this);

  for (auto& callback : onstart_callbacks_)
    std::move(callback).Run();
  onstart_callbacks_.clear();
}

void ArcPaiStarter::OnPaiRequested(mojom::PaiFlowState state) {
  DCHECK(pending_);
  pending_ = false;
  VLOG(1) << "PAI flow state " << state;

  UpdatePlayAutoInstallRequestState(state, profile_);

  if (state != mojom::PaiFlowState::SUCCEEDED) {
    retry_timer_.Start(
        FROM_HERE, retry_interval_,
        base::BindOnce(&ArcPaiStarter::MaybeStartPai, base::Unretained(this)));
    retry_interval_ = std::min(retry_interval_ * 2, kMaxRetryTime);
    return;
  }

  arc::UpdatePlayAutoInstallRequestTime(base::Time::Now() - request_start_time_,
                                        profile_);
  OnPaiDone();
}

void ArcPaiStarter::OnAppRegistered(const std::string& app_id,
                                    const ArcAppListPrefs::AppInfo& app_info) {
  OnAppStatesChanged(app_id, app_info);
}

void ArcPaiStarter::OnAppStatesChanged(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (app_id == kPlayStoreAppId && app_info.ready)
    MaybeStartPai();
}

}  // namespace arc
