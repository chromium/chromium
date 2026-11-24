// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_host.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"

// static
std::unique_ptr<OmniboxPopupWebUIPoolClient>
OmniboxPopupWebUIPoolClient::CreateIfSharingEnabled(
    OmniboxPopupWebUIPoolHost& host,
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(
          omnibox::kOmniboxWebUIPopupShareAcrossWindows) ||
      host.GetSharedPopupKind() == OmniboxPopupState::kNone) {
    return nullptr;
  }
  // Ash internal profiles have no pool.
  OmniboxPopupWebUIPool* pool =
      OmniboxPopupWebUIPoolFactory::GetForProfile(profile);
  if (!pool) {
    return nullptr;
  }
  return std::make_unique<OmniboxPopupWebUIPoolClient>(host, *pool);
}

OmniboxPopupWebUIPoolClient::OmniboxPopupWebUIPoolClient(
    OmniboxPopupWebUIPoolHost& host,
    OmniboxPopupWebUIPool& pool)
    : host_(host), kind_(host.GetSharedPopupKind()), pool_(pool.GetWeakPtr()) {
  pool.AddHost(&host);
  pool.AddClient(this);
  // Hosts are built after the location bar joins the browser window's
  // widget, so this is set outside unit tests.
  if (views::Widget* browser_widget = host.GetBrowserWidget()) {
    browser_widget_observation_.Observe(browser_widget);
  }
}

OmniboxPopupWebUIPoolClient::~OmniboxPopupWebUIPoolClient() {
  if (pool_) {
    pool_->RemoveHost(&*host_, kind_);
    pool_->RemoveClient(this);
  }
}

bool OmniboxPopupWebUIPoolClient::HasSharedWebUIContent() const {
  return pool_ && pool_->HasWebUIContent(*host_);
}

bool OmniboxPopupWebUIPoolClient::SkipPrewarmIfSharedContentExists() {
  if (!HasSharedWebUIContent()) {
    return false;
  }
  RecordContentSource(ContentSource::kPrewarmSkipped);
  return true;
}

std::unique_ptr<WebUIContentsWrapperT<OmniboxPopupUI>>
OmniboxPopupWebUIPoolClient::TakeIdleWebUIContents() {
  return pool_ ? pool_->TakeIdleWebUIContents(*host_) : nullptr;
}

OmniboxPopupWebUIBaseContent*
OmniboxPopupWebUIPoolClient::TakeAndAdoptIdleWebUIContents(
    ContentSource source) {
  const base::TimeTicks start = base::TimeTicks::Now();
  auto contents = TakeIdleWebUIContents();
  if (!contents) {
    return nullptr;
  }
  OmniboxPopupWebUIBaseContent* content =
      host_->AdoptWebUIContents(std::move(contents));
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({host_->GetPopupMetricPrefix(), ".Shared.MoveDuration"}),
      base::TimeTicks::Now() - start);
  RecordContentSource(source);
  return content;
}

void OmniboxPopupWebUIPoolClient::OnWebUIContentCreated() {
  if (pool_ && pool_->ConsumeHolderClosed(kind_)) {
    RecordContentSource(ContentSource::kCreatedAfterHolderClosed);
  } else if (HasSharedWebUIContent()) {
    // The caller failed to take the other window's popup, so it was busy.
    RecordContentSource(ContentSource::kCreatedWhileOtherBusy);
  } else {
    RecordContentSource(ContentSource::kCreated);
  }
}

void OmniboxPopupWebUIPoolClient::RecordContentSource(ContentSource source) {
  base::UmaHistogramEnumeration(
      base::StrCat({host_->GetPopupMetricPrefix(), ".Shared.ContentSource"}),
      source);
}

void OmniboxPopupWebUIPoolClient::RequestWebUIContents(
    base::OnceClosure callback) {
  pending_request_ = std::move(callback);
  RunPendingRequestIfReady();
}

bool OmniboxPopupWebUIPoolClient::RunPendingRequestIfReady() {
  if (!pending_request_) {
    return false;
  }
  if (host_->GetWebUIContent()) {
    pending_request_.Reset();
    return false;
  }
  if (pool_ && pool_->HasLoadingWebUIContent(*host_)) {
    return false;
  }
  std::move(pending_request_).Run();
  return true;
}

void OmniboxPopupWebUIPoolClient::OnWebUIContentsLoaded() {
  if (pool_) {
    pool_->PostRunWaitingClient();
  }
}

void OmniboxPopupWebUIPoolClient::OnPopupHidden() {
  if (OmniboxPopupWebUIBaseContent* content = host_->GetWebUIContent()) {
    if (OmniboxPopupUI* popup_ui =
            content->contents_wrapper()
                ? content->contents_wrapper()->GetWebUIController()
                : nullptr) {
      popup_ui->set_last_hidden_time(base::TimeTicks::Now());
    }
  }
  // Posted so that other windows' popups are not destroyed from inside the
  // hide, which can run from a widget activation change.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OmniboxPopupWebUIPoolClient::DropOtherIdleWebUIContents,
                     weak_factory_.GetWeakPtr()));
}

void OmniboxPopupWebUIPoolClient::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active) {
    return;
  }
  if (pool_) {
    pool_->OnBrowserWindowActivated(widget);
  }
  if (host_->GetWebUIContent()) {
    return;
  }
  // Post rather than move the popup inside the activation change, which is
  // still being dispatched to other observers.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OmniboxPopupWebUIPoolClient::TakeIdleWebUIContentsIfNeeded,
          weak_factory_.GetWeakPtr()));
}

void OmniboxPopupWebUIPoolClient::OnWidgetDestroying(views::Widget* widget) {
  browser_widget_observation_.Reset();
}

void OmniboxPopupWebUIPoolClient::TakeIdleWebUIContentsIfNeeded() {
  if (host_->GetWebUIContent() || RunPendingRequestIfReady()) {
    return;
  }
  TakeAndAdoptIdleWebUIContents(ContentSource::kTakenOnActivation);
}

void OmniboxPopupWebUIPoolClient::DropOtherIdleWebUIContents() {
  if (!pool_ || !host_->GetWebUIContent()) {
    return;
  }
  if (const size_t dropped = pool_->DropOtherIdleWebUIContents(*host_)) {
    base::UmaHistogramExactLinear(base::StrCat({host_->GetPopupMetricPrefix(),
                                                ".Shared.CopiesDroppedOnHide"}),
                                  dropped, 10);
  }
}
