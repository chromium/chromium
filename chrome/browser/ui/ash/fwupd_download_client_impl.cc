// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/fwupd_download_client_impl.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

FwupdDownloadClientImpl::FwupdDownloadClientImpl() = default;
FwupdDownloadClientImpl::~FwupdDownloadClientImpl() = default;

scoped_refptr<network::SharedURLLoaderFactory>
FwupdDownloadClientImpl::GetURLLoaderFactory() {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(active_user);
  DCHECK(profile);

  return profile->GetURLLoaderFactory();
}

}  // namespace ash
