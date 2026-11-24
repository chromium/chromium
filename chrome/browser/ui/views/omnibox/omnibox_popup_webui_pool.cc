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
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_host.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
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

bool IsSameKind(const OmniboxPopupWebUIPoolHost& a,
                const OmniboxPopupWebUIPoolHost& b) {
  return a.GetSharedPopupKind() == b.GetSharedPopupKind();
}

// Returns whether the idle popup of `host` was hidden long enough ago
// for memory pressure to drop it from the active window.
bool HiddenPastActiveWindowTimeout(OmniboxPopupWebUIPoolHost& host) {
  const base::TimeDelta timeout =
      omnibox::kOmniboxWebUIPopupShareActiveWindowPopupIdleTimeout.Get();
  if (timeout.is_max()) {
    return false;
  }
  OmniboxPopupWebUIBaseContent* content = host.GetWebUIContent();
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

void OmniboxPopupWebUIPool::AddHost(OmniboxPopupWebUIPoolHost* host) {
  CHECK_NE(host->GetSharedPopupKind(), OmniboxPopupState::kNone);
  CHECK(!std::ranges::contains(hosts_, host));
  hosts_.push_back(host);
}

void OmniboxPopupWebUIPool::RemoveHost(OmniboxPopupWebUIPoolHost* host,
                                       OmniboxPopupState kind) {
  std::erase(hosts_, host);
  // `host` may be mid-destruction, so its kind is passed in.
  if (host->GetWebUIContent() &&
      std::ranges::none_of(hosts_, [&](const auto& other) {
        return other->GetSharedPopupKind() == kind && other->GetWebUIContent();
      })) {
    holder_closed_kinds_.insert(kind);
  }
}

void OmniboxPopupWebUIPool::AddClient(OmniboxPopupWebUIPoolClient* client) {
  CHECK(!std::ranges::contains(clients_, client));
  clients_.push_back(client);
}

void OmniboxPopupWebUIPool::RemoveClient(OmniboxPopupWebUIPoolClient* client) {
  std::erase(clients_, client);
  // The removed client's popup may have been the one others waited for.
  PostRunWaitingClient();
}

bool OmniboxPopupWebUIPool::HasWebUIContent(
    const OmniboxPopupWebUIPoolHost& host) const {
  return std::ranges::any_of(hosts_, [&](const auto& other) {
    return other != &host && IsSameKind(*other, host) &&
           other->GetWebUIContent();
  });
}

bool OmniboxPopupWebUIPool::HasLoadingWebUIContent(
    const OmniboxPopupWebUIPoolHost& host) const {
  return std::ranges::any_of(hosts_, [&](const auto& other) {
    if (other == &host || !IsSameKind(*other, host)) {
      return false;
    }
    OmniboxPopupWebUIBaseContent* content = other->GetWebUIContent();
    content::WebContents* web_contents =
        content ? content->GetWrappedWebContents() : nullptr;
    return web_contents && web_contents->IsLoading();
  });
}

std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
OmniboxPopupWebUIPool::TakeIdleWebUIContents(
    const OmniboxPopupWebUIPoolHost& host) {
  for (OmniboxPopupWebUIPoolHost* other : hosts_) {
    if (other != &host && IsSameKind(*other, host) &&
        other->CanReleaseWebUIContents()) {
      return other->ReleaseWebUIContents();
    }
  }
  return nullptr;
}

size_t OmniboxPopupWebUIPool::DropOtherIdleWebUIContents(
    const OmniboxPopupWebUIPoolHost& host) {
  size_t dropped = 0;
  for (OmniboxPopupWebUIPoolHost* other : hosts_) {
    if (other != &host && IsSameKind(*other, host) &&
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

void OmniboxPopupWebUIPool::PostRunWaitingClient() {
  // Posted so that a popup does not move inside its own page's load
  // notification or its window's teardown.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&OmniboxPopupWebUIPool::RunWaitingClient,
                                weak_factory_.GetWeakPtr()));
}

void OmniboxPopupWebUIPool::OnBrowserWindowActivated(
    views::Widget* browser_widget) {
  last_active_browser_widget_ = browser_widget->GetWeakPtr();
}

void OmniboxPopupWebUIPool::DropIdleWebUIContents() {
  views::Widget* last_active_browser_widget = last_active_browser_widget_.get();
  for (OmniboxPopupWebUIPoolHost* host : hosts_) {
    if (host->GetSharedPopupKind() != OmniboxPopupState::kAim &&
        last_active_browser_widget &&
        host->GetBrowserWidget() == last_active_browser_widget &&
        !HiddenPastActiveWindowTimeout(*host)) {
      continue;
    }
    if (host->CanReleaseWebUIContents()) {
      // Destroys the released WebContents.
      host->ReleaseWebUIContents();
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

void OmniboxPopupWebUIPool::RunWaitingClient() {
  views::Widget* active_widget = last_active_browser_widget_.get();
  for (OmniboxPopupWebUIPoolClient* client : clients_) {
    if ((!active_widget || client->browser_widget() == active_widget) &&
        client->RunPendingRequestIfReady()) {
      return;
    }
  }
}
