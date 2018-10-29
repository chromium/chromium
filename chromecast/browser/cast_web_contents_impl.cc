// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents_impl.h"

#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace chromecast {

CastWebContentsImpl::CastWebContentsImpl(
    CastWebContentsImpl::Delegate* delegate,
    content::WebContents* web_contents,
    bool enabled_for_dev)
    : delegate_(delegate),
      web_contents_(web_contents),
      page_state_(PageState::IDLE),
      enabled_for_dev_(enabled_for_dev),
      remote_debugging_server_(
          shell::CastBrowserProcess::GetInstance()->remote_debugging_server()),
      closing_(false),
      stopped_(false),
      stop_notified_(false),
      notifying_(false),
      last_error_(net::OK),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK(web_contents_);
  DCHECK(web_contents_->GetController().IsInitialNavigation());
  DCHECK(!web_contents_->IsLoading());
  content::WebContentsObserver::Observe(web_contents_);
  if (enabled_for_dev_) {
    LOG(INFO) << "Enabling dev console for CastWebContentsImpl";
    remote_debugging_server_->EnableWebContentsForDebugging(web_contents_);
  }
  delegate_->OnPageStateChanged(this);
}

CastWebContentsImpl::~CastWebContentsImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DisableDebugging();
}

content::WebContents* CastWebContentsImpl::web_contents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_contents_;
}

CastWebContents::PageState CastWebContentsImpl::page_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_state_;
}

void CastWebContentsImpl::LoadUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_) {
    LOG(ERROR) << "Cannot load URL for deleted WebContents";
    return;
  }
  if (closing_) {
    LOG(ERROR) << "Cannot load URL for WebContents while closing";
    return;
  }
  closing_ = false;
  stopped_ = false;
  stop_notified_ = false;
  last_error_ = net::OK;
  start_loading_ticks_ = base::TimeTicks::Now();
  LOG(INFO) << "Load url: " << url.possibly_invalid_spec();
  TracePageLoadBegin(url);
  web_contents_->GetController().LoadURL(url, content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  UpdatePageState();
  DCHECK_EQ(PageState::LOADING, page_state_);
}

void CastWebContentsImpl::ClosePage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_ || closing_)
    return;
  closing_ = true;
  web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  web_contents_->ClosePage();
  // If the WebContents doesn't close within the specified timeout, then signal
  // the page closure anyway so that the Delegate can delete the WebContents and
  // stop the page itself.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebContentsImpl::OnClosePageTimeout,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1000));
}

void CastWebContentsImpl::Stop(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    UpdatePageState();
    return;
  }
  last_error_ = error_code;
  closing_ = false;
  stopped_ = true;
  UpdatePageState();
  DCHECK_NE(PageState::IDLE, page_state_);
  DCHECK_NE(PageState::LOADING, page_state_);
  DCHECK_NE(PageState::LOADED, page_state_);
}

void CastWebContentsImpl::SetDelegate(CastWebContents::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

void CastWebContentsImpl::OnClosePageTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!closing_ || stopped_) {
    return;
  }
  closing_ = false;
  Stop(net::OK);
}

void CastWebContentsImpl::RenderProcessGone(base::TerminationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Render process for main frame exited unexpectedly.";
  Stop(net::ERR_UNEXPECTED);
}

void CastWebContentsImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Ignore these navigations.
  if (!navigation_handle->HasCommitted()) {
    LOG(WARNING) << "Navigation did not commit: url="
                 << navigation_handle->GetURL();
    return;
  }

  if (!navigation_handle->IsErrorPage())
    return;

  net::Error error_code = navigation_handle->GetNetErrorCode();

  // If we abort errors in an iframe, it can create a really confusing
  // and fragile user experience.  Rather than create a list of errors
  // that are most likely to occur, we ignore all of them for now.
  if (!navigation_handle->IsInMainFrame()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << navigation_handle->GetURL()
               << ", error=" << error_code
               << ", description=" << net::ErrorToShortString(error_code);
    return;
  }

  LOG(ERROR) << "Got error on navigation: url=" << navigation_handle->GetURL()
             << ", error_code=" << error_code
             << ", description= " << net::ErrorToShortString(error_code);

  Stop(error_code);
  DCHECK_EQ(page_state_, PageState::ERROR);
}

void CastWebContentsImpl::DidStartLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdatePageState();
  DCHECK_EQ(page_state_, PageState::LOADING);
}

void CastWebContentsImpl::DidStopLoading() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int http_status_code = 0;
  GURL final_url;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    http_status_code = nav_entry->GetHttpStatusCode();
    final_url = nav_entry->GetVirtualURL();
  }
  TracePageLoadEnd(final_url);

  if (http_status_code != 0 && http_status_code / 100 != 2) {
    // We successfully loaded an error HTML page.
    LOG(INFO) << "Failed loading page for: " << final_url
              << "; http status code: " << http_status_code;
    Stop(net::ERR_FAILED);
    DCHECK_EQ(page_state_, PageState::ERROR);
    return;
  }
  // Main frame finished loading.
  base::TimeDelta load_time = base::TimeTicks::Now() - start_loading_ticks_;
  LOG(INFO) << "Finished loading page after " << load_time.InMilliseconds()
            << " ms, url=" << final_url;
  PageState previous = page_state_;
  UpdatePageState();
  DCHECK((previous == PageState::ERROR && page_state_ == PageState::ERROR) ||
         page_state_ == PageState::LOADED)
      << "Page is in unexpected state: " << page_state_;
}

void CastWebContentsImpl::UpdatePageState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PageState last_state = page_state_;
  if (!web_contents_) {
    DCHECK(stopped_);
    page_state_ = PageState::DESTROYED;
  } else if (!stopped_) {
    if (web_contents_->IsLoading()) {
      page_state_ = PageState::LOADING;
    } else {
      page_state_ = PageState::LOADED;
    }
  } else if (stopped_) {
    if (last_error_ != net::OK) {
      page_state_ = PageState::ERROR;
    } else {
      page_state_ = PageState::CLOSED;
    }
  }

  if (!delegate_)
    return;
  // Don't notify if the page state didn't change.
  if (last_state == page_state_)
    return;
  // Don't recursively notify the delegate.
  if (notifying_)
    return;
  notifying_ = true;
  if (stopped_ && !stop_notified_) {
    stop_notified_ = true;
    delegate_->OnPageStopped(this, last_error_);
  } else {
    delegate_->OnPageStateChanged(this);
  }
  notifying_ = false;
}

void CastWebContentsImpl::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only report an error if we are the main frame.  See b/8433611.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
               << ", error=" << error_code;
    return;
  }
  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED means download was aborted by the app, typically this happens
    // when flinging URL for direct playback, the initial URLRequest gets
    // cancelled/aborted and then the same URL is requested via the buffered
    // data source for media::Pipeline playback.
    LOG(INFO) << "Load canceled: url=" << validated_url.spec();
    return;
  }

  LOG(ERROR) << "Got error on load: url=" << validated_url.spec()
             << ", error_code=" << error_code;

  TracePageLoadEnd(validated_url);
  Stop(error_code);
  DCHECK_EQ(PageState::ERROR, page_state_);
}

void CastWebContentsImpl::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closing_ = false;
  DisableDebugging();
  content::WebContentsObserver::Observe(nullptr);
  web_contents_ = nullptr;
  Stop(net::OK);
  DCHECK_EQ(PageState::DESTROYED, page_state_);
}

void CastWebContentsImpl::TracePageLoadBegin(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_ASYNC_BEGIN1("browser,navigation", "CastWebContentsImpl Launch",
                           this, "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::TracePageLoadEnd(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_ASYNC_END1("browser,navigation", "CastWebContentsImpl Launch",
                         this, "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::DisableDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enabled_for_dev_ || !web_contents_)
    return;
  LOG(INFO) << "Disabling dev console for " << web_contents_->GetVisibleURL();
  remote_debugging_server_->DisableWebContentsForDebugging(web_contents_);
}

std::ostream& operator<<(std::ostream& os,
                         CastWebContentsImpl::PageState state) {
#define CASE(state)                           \
  case CastWebContentsImpl::PageState::state: \
    os << #state;                             \
    return os;

  switch (state) {
    CASE(IDLE);
    CASE(LOADING);
    CASE(LOADED);
    CASE(CLOSED);
    CASE(DESTROYED);
    CASE(ERROR);
  }
#undef CASE
}

}  // namespace chromecast
