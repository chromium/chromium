// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_client.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_base.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_pool_factory.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "components/omnibox/common/omnibox_features.h"

// static
std::unique_ptr<OmniboxPopupWebUIPoolClient>
OmniboxPopupWebUIPoolClient::CreateIfSharingEnabled(
    OmniboxPopupPresenterBase& presenter,
    Profile* profile,
    views::Widget* browser_widget) {
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
  return std::make_unique<OmniboxPopupWebUIPoolClient>(presenter, *pool,
                                                       browser_widget);
}

OmniboxPopupWebUIPoolClient::OmniboxPopupWebUIPoolClient(
    OmniboxPopupPresenterBase& presenter,
    OmniboxPopupWebUIPool& pool,
    views::Widget* browser_widget)
    : presenter_(presenter), pool_(pool.GetWeakPtr()) {
  pool.AddPresenter(&presenter);
  // Presenters are built after the location bar joins the browser window's
  // widget, so this is set outside unit tests.
  if (browser_widget) {
    browser_widget_observation_.Observe(browser_widget);
  }
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

void OmniboxPopupWebUIPoolClient::OnPopupHidden() {
  if (OmniboxPopupWebUIBaseContent* content = presenter_->GetWebUIContent()) {
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
  if (presenter_->GetWebUIContent()) {
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
  if (presenter_->GetWebUIContent()) {
    return;
  }
  if (auto contents = TakeIdleWebUIContents()) {
    presenter_->AdoptWebUIContents(std::move(contents));
  }
}

void OmniboxPopupWebUIPoolClient::DropOtherIdleWebUIContents() {
  if (pool_ && presenter_->GetWebUIContent()) {
    pool_->DropOtherIdleWebUIContents(*presenter_);
  }
}
