// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_view_default.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/browser/lru_renderer_cache.h"
#include "chromecast/browser/webui/cast_webui_controller_factory.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace chromecast {

namespace {

uint32_t remove_data_mask =
    content::StoragePartition::REMOVE_DATA_MASK_COOKIES |
    content::StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS |
    content::StoragePartition::REMOVE_DATA_MASK_INDEXEDDB |
    content::StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE |
    content::StoragePartition::REMOVE_DATA_MASK_WEBSQL;

}  // namespace

CastWebService::CastWebService(content::BrowserContext* browser_context,
                               CastWindowManager* window_manager)
    : browser_context_(browser_context),
      window_manager_(window_manager),
      default_web_view_factory_(browser_context),
      override_web_view_factory_(nullptr),
      overlay_renderer_cache_(
          std::make_unique<LRURendererCache>(browser_context_, 1)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {
  DCHECK(browser_context_);
  DCHECK(task_runner_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

CastWebService::~CastWebService() = default;

void CastWebService::OverrideWebViewFactory(
    CastWebViewFactory* web_view_factory) {
  override_web_view_factory_ = web_view_factory;
}

CastWebView::Scoped CastWebService::CreateWebViewInternal(
    mojom::CastWebViewParamsPtr params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastWebViewFactory* web_view_factory = override_web_view_factory_;
  if (!web_view_factory) {
    web_view_factory = &default_web_view_factory_;
  }
  auto web_view = web_view_factory->CreateWebView(std::move(params), this);
  CastWebView::Scoped scoped(web_view.release(), [this](CastWebView* web_view) {
    OwnerDestroyed(web_view);
  });

  return scoped;
}

void CastWebService::CreateWebView(
    mojom::CastWebViewParamsPtr params,
    mojo::PendingReceiver<mojom::CastWebContents> web_contents,
    mojo::PendingReceiver<mojom::CastContentWindow> window) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CastWebViewFactory* web_view_factory = override_web_view_factory_;
  if (!web_view_factory) {
    web_view_factory = &default_web_view_factory_;
  }
  auto web_view = web_view_factory->CreateWebView(std::move(params), this);
  web_view->cast_web_contents()->SetDisconnectCallback(base::BindOnce(
      &CastWebService::OwnerDestroyed, base::Unretained(this), web_view.get()));
  web_view->BindReceivers(std::move(web_contents), std::move(window));
  web_views_.insert(std::move(web_view));
}

void CastWebService::FlushDomLocalStorage() {
  browser_context_->ForEachLoadedStoragePartition(
      &content::StoragePartition::Flush);
}

void CastWebService::ClearLocalStorage(ClearLocalStorageCallback callback) {
  // TODO(crbug.com/40944952): Only the first StoragePartition gets a
  // non-null `callback`; the subsequent ones all get a null callback, so this
  // only ends up waiting for the first storage partition beofre invoking the
  // reply callback.
  browser_context_->ForEachLoadedStoragePartition(
      [&](content::StoragePartition* partition) {
        auto cookie_delete_filter = network::mojom::CookieDeletionFilter::New();
        cookie_delete_filter->session_control =
            network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL;
        partition->ClearData(
            remove_data_mask,
            content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
            /*filter_builder=*/nullptr,
            content::StoragePartition::StorageKeyPolicyMatcherFunction(),
            std::move(cookie_delete_filter), /*perform_cleanup=*/true,
            base::Time::Min(), base::Time::Max(), std::move(callback));
      });
}

bool CastWebService::IsCastWebUIOrigin(const url::Origin& origin) {
  return base::Contains(cast_webui_hosts_, origin.host());
}

void CastWebService::RegisterWebUiClient(
    mojo::PendingRemote<mojom::WebUiClient> client,
    const std::vector<std::string>& hosts) {
  cast_webui_hosts_ = hosts;
  content::WebUIControllerFactory::RegisterFactory(
      new CastWebUiControllerFactory(std::move(client), hosts));
}

void CastWebService::DeleteOwnedWebViews() {
  DCHECK(!immediately_delete_webviews_);
  // We don't want to delay webview deletion after this point.
  immediately_delete_webviews_ = true;
  web_views_.clear();
}

void CastWebService::OwnerDestroyed(CastWebView* web_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_view->OwnerDestroyed();
  content::WebContents* web_contents = web_view->web_contents();
  GURL url;
  if (web_contents) {
    url = web_contents->GetVisibleURL();
    // Suspend the MediaSession to free up media resources for the next content
    // window.
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
  if (!base::Contains(web_views_, web_view, &std::unique_ptr<CastWebView>::get))
    web_views_.emplace(web_view);
  auto delay = web_view->shutdown_delay();
  if (delay <= base::TimeDelta() || immediately_delete_webviews_) {
    LOG(INFO) << "Immediately deleting CastWebView for " << url;
    DeleteWebView(web_view);
    return;
  }
  LOG(INFO) << "Deleting CastWebView for " << url << " in "
            << delay.InMilliseconds() << " milliseconds.";
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebService::DeleteWebView, weak_ptr_, web_view),
      delay);
}

void CastWebService::DeleteWebView(CastWebView* web_view) {
  LOG(INFO) << "Deleting CastWebView.";
  base::EraseIf(web_views_,
                [web_view](const std::unique_ptr<CastWebView>& ptr) {
                  return ptr.get() == web_view;
                });
}

}  // namespace chromecast
