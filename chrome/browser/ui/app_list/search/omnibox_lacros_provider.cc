// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_lacros_provider.h"

#include "base/bind.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/search_provider_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"

namespace app_list {

OmniboxLacrosProvider::OmniboxLacrosProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : profile_(profile), list_controller_(list_controller) {
  DCHECK(profile_);
  DCHECK(list_controller_);

  if (crosapi::CrosapiManager::IsInitialized()) {
    search_provider_ =
        crosapi::CrosapiManager::Get()->crosapi_ash()->search_provider_ash();
    DCHECK(search_provider_);
  }
}

OmniboxLacrosProvider::~OmniboxLacrosProvider() = default;

void OmniboxLacrosProvider::Start(const std::u16string& query) {
  if (!search_provider_)
    return;

  search_provider_->Search(
      query, base::BindRepeating(&OmniboxLacrosProvider::OnResultsReceived,
                                 weak_factory_.GetWeakPtr()));
}

ash::AppListSearchResultType OmniboxLacrosProvider::ResultType() const {
  return ash::AppListSearchResultType::kOmnibox;
}

void OmniboxLacrosProvider::OnResultsReceived(
    std::vector<crosapi::mojom::SearchResultPtr> results) {
  // TODO(crbug.com/1228587): Implement.
}

}  // namespace app_list
