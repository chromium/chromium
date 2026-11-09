// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    // Each popup page takes a few MB, and the popup renderer process more
    // once no popup is left in it.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    // The memory is the renderer's, freed when the WebContents goes away.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    // Idle popups hold no state for any window.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    // The next popup loads a new page.
    base::MemoryConsumerTraits::RecreateMemoryCost::kExpensive,
    // Popup pages live in a renderer process.
    base::MemoryConsumerTraits::InProcess::kNo,
    // Each release drops the idle popups once, with no lasting limit.
    base::MemoryConsumerTraits::IsStateful::kNo);

bool IsSameKind(const OmniboxPopupPresenterBase& a,
                const OmniboxPopupPresenterBase& b) {
  return a.GetSharedPopupKind() == b.GetSharedPopupKind();
}

views::Widget* GetBrowserWidget(const OmniboxPopupPresenterBase& presenter) {
  views::Widget* widget = presenter.delegate().GetLocationBarWidget();
  return widget ? widget->GetTopLevelWidget() : nullptr;
}

// Returns whether the idle popup of `presenter` was hidden long enough ago
// for memory pressure to drop it from the active window.
bool HiddenPastActiveWindowTimeout(OmniboxPopupPresenterBase& presenter) {
  const base::TimeDelta timeout =
      omnibox::kOmniboxWebUIPopupShareActiveWindowPopupIdleTimeout.Get();
  if (timeout.is_max()) {
    return false;
  }
  OmniboxPopupWebUIBaseContent* content = presenter.GetWebUIContent();
  OmniboxPopupUI* popup_ui =
      content && content->contents_wrapper()
          ? content->contents_wrapper()->GetWebUIController()
          : nullptr;
  if (!popup_ui) {
    return false;
  }
  // A popup that was never shown has been idle since it loaded.
  const base::TimeTicks hidden = popup_ui->last_hidden_time();
  return hidden.is_null() || base::TimeTicks::Now() - hidden >= timeout;
}

}  // namespace

OmniboxPopupWebUIPool::OmniboxPopupWebUIPool() {
  if (base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupShareAcrossWindows) &&
      omnibox::kOmniboxWebUIPopupShareDropIdleOnMemoryPressure.Get()) {
    memory_consumer_registration_.emplace("OmniboxPopupWebUIPool",
                                          kMemoryConsumerTraits, this);
  }
}

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
    OmniboxPopupPresenterBase* presenter,
    OmniboxPopupState kind) {
  std::erase(presenters_, presenter);
  // `presenter` may be mid-destruction, so its kind is passed in.
  if (presenter->GetWebUIContent() &&
      std::ranges::none_of(presenters_, [&](const auto& other) {
        return other->GetSharedPopupKind() == kind && other->GetWebUIContent();
      })) {
    holder_closed_kinds_.insert(kind);
  }
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

size_t OmniboxPopupWebUIPool::DropOtherIdleWebUIContents(
    const OmniboxPopupPresenterBase& presenter) {
  size_t dropped = 0;
  for (OmniboxPopupPresenterBase* other : presenters_) {
    if (other != &presenter && IsSameKind(*other, presenter) &&
        other->CanReleaseWebUIContents()) {
      // Destroys the released WebContents.
      other->ReleaseWebUIContents();
      ++dropped;
    }
  }
  return dropped;
}

bool OmniboxPopupWebUIPool::ConsumeHolderClosed(OmniboxPopupState kind) {
  return holder_closed_kinds_.erase(kind) > 0;
}

void OmniboxPopupWebUIPool::OnBrowserWindowActivated(
    views::Widget* browser_widget) {
  last_active_browser_widget_ = browser_widget->GetWeakPtr();
}

void OmniboxPopupWebUIPool::DropIdleWebUIContents() {
  views::Widget* last_active_browser_widget = last_active_browser_widget_.get();
  for (OmniboxPopupPresenterBase* presenter : presenters_) {
    if (presenter->GetSharedPopupKind() != OmniboxPopupState::kAim &&
        last_active_browser_widget &&
        GetBrowserWidget(*presenter) == last_active_browser_widget &&
        !HiddenPastActiveWindowTimeout(*presenter)) {
      continue;
    }
    if (presenter->CanReleaseWebUIContents()) {
      // Destroys the released WebContents.
      presenter->ReleaseWebUIContents();
    }
  }
}

// static
bool OmniboxPopupWebUIPool::ShouldDropIdleWebUIContents(
    base::MemoryLimit limit) {
  return limit <=
         (omnibox::kOmniboxWebUIPopupShareDropIdleOnModeratePressure.Get()
              ? base::MemoryLimit::ModeratePressureThreshold()
              : base::MemoryLimit::CriticalPressureThreshold());
}

void OmniboxPopupWebUIPool::OnReleaseMemory() {
  if (ShouldDropIdleWebUIContents(memory_limit())) {
    DropIdleWebUIContents();
  }
}

void OmniboxPopupWebUIPool::OnUpdateMemoryLimit() {}
