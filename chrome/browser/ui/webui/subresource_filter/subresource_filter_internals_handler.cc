// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace subresource_filter {

SubresourceFilterInternalsHandler::SubresourceFilterInternalsHandler(
    Profile* profile)
    : profile_(profile) {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSubresourceFilterHighlightAds,
      base::BindRepeating(&SubresourceFilterInternalsHandler::OnPrefChanged,
                          base::Unretained(this)));
}

SubresourceFilterInternalsHandler::~SubresourceFilterInternalsHandler() =
    default;

void SubresourceFilterInternalsHandler::BindInterface(
    mojo::PendingReceiver<mojom::SubresourceFilterInternalsHandler> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void SubresourceFilterInternalsHandler::GetInternalsPageSettings(
    GetInternalsPageSettingsCallback callback) {
  auto settings = mojom::SubresourceFilterInternalsPageSettings::New(
      profile_->GetPrefs()->GetBoolean(prefs::kSubresourceFilterHighlightAds));
  std::move(callback).Run(std::move(settings));
}

void SubresourceFilterInternalsHandler::SetInternalsPageSettings(
    mojom::SubresourceFilterInternalsPageSettingsPtr settings) {
  profile_->GetPrefs()->SetBoolean(prefs::kSubresourceFilterHighlightAds,
                                   settings->should_highlight_ads);
}

void SubresourceFilterInternalsHandler::ObserveInternalsPageSettings(
    mojo::PendingRemote<mojom::SubresourceFilterInternalsObserver> observer) {
  observer_.reset();
  observer_.Bind(std::move(observer));
}

void SubresourceFilterInternalsHandler::OnPrefChanged() {
  if (observer_.is_bound()) {
    auto settings = mojom::SubresourceFilterInternalsPageSettings::New(
        profile_->GetPrefs()->GetBoolean(
            prefs::kSubresourceFilterHighlightAds));
    observer_->OnInternalsPageSettingsChanged(std::move(settings));
  }
}

}  // namespace subresource_filter
