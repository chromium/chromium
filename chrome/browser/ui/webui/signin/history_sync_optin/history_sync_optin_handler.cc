// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

HistorySyncOptinHandler::HistorySyncOptinHandler(
    mojo::PendingReceiver<history_sync_optin::mojom::PageHandler> receiver,
    mojo::PendingRemote<history_sync_optin::mojom::Page> page,
    Browser* browser,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser ? browser->AsWeakPtr() : nullptr),
      profile_(profile) {
  CHECK(profile_);
}

HistorySyncOptinHandler::~HistorySyncOptinHandler() = default;

void HistorySyncOptinHandler::Accept() {
  AddHistorySyncConsent();
  FinishAndCloseDialog();
}

void HistorySyncOptinHandler::Reject() {
  FinishAndCloseDialog();
}

void HistorySyncOptinHandler::FinishAndCloseDialog() {
  // TODO(crbug.com/404806506): Add metrics.
  if (browser_) {
    browser_->signin_view_controller()->CloseModalSignin();
  }
}

void HistorySyncOptinHandler::AddHistorySyncConsent() {
  sync_pb::UserConsentTypes::SyncConsent sync_consent;
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  CHECK(sync_service);
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  // TODO(crbug.com/404806988): As we add the invocation points check if additional actions
  // are needed to enable sync for history.
  // The invocation below works for an already syncing user. It enables the syncing for history
  // if it's not already turned on.
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, /*is_type_on=*/true);
}
