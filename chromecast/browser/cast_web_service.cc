// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_service.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/browser/cast_web_view_default.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {

CastWebService::CastWebService(content::BrowserContext* browser_context,
                               CastWebViewFactory* web_view_factory,
                               CastWindowManager* window_manager)
    : browser_context_(browser_context),
      web_view_factory_(web_view_factory),
      window_manager_(window_manager),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(browser_context_);
  DCHECK(web_view_factory_);
  DCHECK(task_runner_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

CastWebService::~CastWebService() = default;

CastWebView::Scoped CastWebService::CreateWebView(
    const CastWebView::CreateParams& params,
    scoped_refptr<content::SiteInstance> site_instance,
    const GURL& initial_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto web_view = web_view_factory_->CreateWebView(
      params, this, std::move(site_instance), initial_url);
  CastWebView::Scoped scoped(web_view.get(), [this](CastWebView* web_view) {
    OwnerDestroyed(web_view);
  });
  web_views_.insert(std::move(web_view));
  return scoped;
}

CastWebView::Scoped CastWebService::CreateWebView(
    const CastWebView::CreateParams& params,
    const GURL& initial_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto web_view = web_view_factory_->CreateWebView(
      params, this,
      content::SiteInstance::CreateForURL(browser_context_, initial_url),
      initial_url);
  CastWebView::Scoped scoped(web_view.get(), [this](CastWebView* web_view) {
    OwnerDestroyed(web_view);
  });
  web_views_.insert(std::move(web_view));
  return scoped;
}

void CastWebService::OwnerDestroyed(CastWebView* web_view) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContents* web_contents = web_view->web_contents();
  GURL url;
  if (web_contents) {
    url = web_contents->GetVisibleURL();
    // Suspend the MediaSession to free up media resources for the next content
    // window.
    content::MediaSession::Get(web_contents)
        ->Suspend(content::MediaSession::SuspendType::kSystem);
  }
  auto delay = web_view->shutdown_delay();
  if (delay <= base::TimeDelta()) {
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
