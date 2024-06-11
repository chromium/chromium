// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/chrome_focus_mode_delegate.h"

#include "ash/shell.h"
#include "ash/system/focus_mode/sounds/youtube_music/youtube_music_client.h"
#include "base/functional/bind.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {

// This will be used as a callback that is passed to the client to generate
// `google_apis::RequestSender`. The request sender class implements the
// OAuth 2.0 protocol, so we do not need to do anything extra for authentication
// and authorization.
//   `scopes`:
//     The OAuth scopes.
//   `traffic_annotation_tag`:
//     It documents the network request for system admins and regulators.
std::unique_ptr<google_apis::RequestSender> CreateRequestSenderForClient(
    const std::vector<std::string>& scopes,
    const net::NetworkTrafficAnnotationTag& traffic_annotation_tag) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetURLLoaderFactory();

  std::unique_ptr<google_apis::AuthService> auth_service =
      std::make_unique<google_apis::AuthService>(
          identity_manager,
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          url_loader_factory, scopes);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
              to the user on System UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/std::string(), traffic_annotation_tag);
}

}  // namespace

ChromeFocusModeDelegate::ChromeFocusModeDelegate() = default;

ChromeFocusModeDelegate::~ChromeFocusModeDelegate() = default;

std::unique_ptr<ash::youtube_music::YouTubeMusicClient>
ChromeFocusModeDelegate::CreateYouTubeMusicClient() {
  return std::make_unique<ash::youtube_music::YouTubeMusicClient>(
      base::BindRepeating(&CreateRequestSenderForClient));
}
