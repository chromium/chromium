// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"

// static
std::unique_ptr<OmniboxPopupWebUIPoolClient>
OmniboxPopupWebUIPoolClient::CreateIfSharingEnabled(
    OmniboxPopupPresenterBase& presenter,
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupShareAcrossWindows) ||
      presenter.GetSharedPopupKind() == OmniboxPopupState::kNone) {
    return nullptr;
  }
  // Ash internal profiles have no pool.
  OmniboxPopupWebUIPool* pool =
      OmniboxPopupWebUIPoolFactory::GetForProfile(profile);
  if (!pool) {
    return nullptr;
  }
  return std::make_unique<OmniboxPopupWebUIPoolClient>(presenter, *pool);
}

OmniboxPopupWebUIPoolClient::OmniboxPopupWebUIPoolClient(
    OmniboxPopupPresenterBase& presenter,
    OmniboxPopupWebUIPool& pool)
    : presenter_(presenter), pool_(pool.GetWeakPtr()) {
  pool.AddPresenter(&presenter);
}

OmniboxPopupWebUIPoolClient::~OmniboxPopupWebUIPoolClient() {
  if (pool_) {
    pool_->RemovePresenter(&*presenter_);
  }
}

bool OmniboxPopupWebUIPoolClient::HasSharedWebUIContent() const {
  return pool_ && pool_->HasWebUIContent(*presenter_);
}

std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
OmniboxPopupWebUIPoolClient::TakeIdleWebUIContents() {
  return pool_ ? pool_->TakeIdleWebUIContents(*presenter_) : nullptr;
}
