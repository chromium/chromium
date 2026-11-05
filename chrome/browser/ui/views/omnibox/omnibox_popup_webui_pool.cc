// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

namespace {

bool IsSameKind(const OmniboxPopupPresenterBase& a,
                const OmniboxPopupPresenterBase& b) {
  return a.GetSharedPopupKind() == b.GetSharedPopupKind();
}

}  // namespace

OmniboxPopupWebUIPool::OmniboxPopupWebUIPool() = default;

OmniboxPopupWebUIPool::~OmniboxPopupWebUIPool() = default;

base::WeakPtr<OmniboxPopupWebUIPool> OmniboxPopupWebUIPool::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void OmniboxPopupWebUIPool::AddPresenter(OmniboxPopupPresenterBase* presenter) {
  CHECK_NE(presenter->GetSharedPopupKind(), OmniboxPopupState::kNone);
  CHECK(!std::ranges::contains(presenters_, presenter));
  presenters_.push_back(presenter);
}

void OmniboxPopupWebUIPool::RemovePresenter(
    OmniboxPopupPresenterBase* presenter) {
  std::erase(presenters_, presenter);
}

bool OmniboxPopupWebUIPool::HasWebUIContent(
    const OmniboxPopupPresenterBase& presenter) const {
  return std::ranges::any_of(presenters_, [&](const auto& other) {
    return other != &presenter && IsSameKind(*other, presenter) &&
           other->GetWebUIContent();
  });
}

std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
OmniboxPopupWebUIPool::TakeIdleWebUIContents(
    const OmniboxPopupPresenterBase& presenter) {
  for (OmniboxPopupPresenterBase* other : presenters_) {
    if (other != &presenter && IsSameKind(*other, presenter) &&
        other->CanReleaseWebUIContents()) {
      return other->ReleaseWebUIContents();
    }
  }
  return nullptr;
}

void OmniboxPopupWebUIPool::DropOtherIdleWebUIContents(
    const OmniboxPopupPresenterBase& presenter) {
  for (OmniboxPopupPresenterBase* other : presenters_) {
    if (other != &presenter && IsSameKind(*other, presenter) &&
        other->CanReleaseWebUIContents()) {
      // Destroys the released WebContents.
      other->ReleaseWebUIContents();
    }
  }
}
