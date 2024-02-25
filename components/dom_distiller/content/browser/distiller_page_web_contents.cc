// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_constants.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace dom_distiller {

SourcePageHandleWebContents::SourcePageHandleWebContents(
    content::WebContents* web_contents,
    bool owned)
    : web_contents_(web_contents), owned_(owned) {
  if (web_contents_ && owned) {
    web_contents_->SetOwnerLocationForDebug(FROM_HERE);
  }
}

SourcePageHandleWebContents::~SourcePageHandleWebContents() {
  if (owned_) {
    delete web_contents_;
  }
}

std::unique_ptr<DistillerPage>
DistillerPageWebContentsFactory::CreateDistillerPage(
    const gfx::Size& render_view_size) const {
  DCHECK(browser_context_);
  return std::unique_ptr<DistillerPage>(new DistillerPageWebContents(
      browser_context_, render_view_size,
      std::unique_ptr<SourcePageHandleWebContents>()));
}

std::unique_ptr<DistillerPage>
DistillerPageWebContentsFactory::CreateDistillerPageWithHandle(
    std::unique_ptr<SourcePageHandle> handle) const {
  DCHECK(browser_context_);
  std::unique_ptr<SourcePageHandleWebContents> web_contents_handle =
      std::unique_ptr<SourcePageHandleWebContents>(
          static_cast<SourcePageHandleWebContents*>(handle.release()));
  return std::unique_ptr<DistillerPage>(new DistillerPageWebContents(
      browser_context_, gfx::Size(), std::move(web_contents_handle)));
}

DistillerPageWebContents::DistillerPageWebContents(
    content::BrowserContext* browser_context,
    const gfx::Size& render_view_size,
    std::unique_ptr<SourcePageHandleWebContents> optional_web_contents_handle)
    : state_(IDLE),
      source_page_handle_(nullptr),
      browser_context_(browser_context),
      render_view_size_(render_view_size) {
  if (optional_web_contents_handle) {
    source_page_handle_ = std::move(optional_web_contents_handle);
    if (render_view_size.IsEmpty())
      render_view_size_ =
          source_page_handle_->web_contents()->GetContainerBounds().size();
  }
}

DistillerPageWebContents::~DistillerPageWebContents() = default;

bool DistillerPageWebContents::StringifyOutput() {
  return false;
}

void DistillerPageWebContents::DistillPageImpl(const GURL& url,
                                               const std::string& script) {
  DCHECK(browser_context_);
  DCHECK(state_ == IDLE);
  state_ = LOADING_PAGE;
  script_ = script;

  if (source_page_handle_ && source_page_handle_->web_contents() &&
      TargetRenderFrameHost().GetLastCommittedURL() == url) {
    if (TargetRenderFrameHost().IsDOMContentLoaded()) {
      // Main frame has already loaded for the current WebContents, so execute
      // JavaScript immediately.
      ExecuteJavaScript();
    } else {
      // Main frame document has not loaded yet, so wait until it has before
      // executing JavaScript. It will trigger after DOMContentLoaded is
      // called for the main frame.
      content::WebContentsObserver::Observe(
          source_page_handle_->web_contents());
    }
  } else {
    CreateNewWebContents(url);
  }
}

void DistillerPageWebContents::CreateNewWebContents(const GURL& url) {
  // Create new WebContents to use for distilling the content.
  content::WebContents::CreateParams create_params(browser_context_);
  create_params.initially_hidden = true;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  DCHECK(web_contents);

  web_contents->SetDelegate(this);

  // Start observing WebContents and load the requested URL.
  content::WebContentsObserver::Observe(web_contents.get());
  content::NavigationController::LoadURLParams params(url);
  web_contents->GetController().LoadURLWithParams(params);

  // SourcePageHandleWebContents takes ownership of |web_contents|.
  source_page_handle_ = std::make_unique<SourcePageHandleWebContents>(
      web_contents.release(), true);
}

gfx::Size DistillerPageWebContents::GetSizeForNewRenderView(
    content::WebContents* web_contents) {
  gfx::Size size(render_view_size_);
  if (size.IsEmpty())
    size = web_contents->GetContainerBounds().size();
  // If size is still empty, set it to fullscreen so that document.offsetWidth
  // in the executed domdistiller.js won't be 0.
  if (size.IsEmpty()) {
    DVLOG(1) << "Using fullscreen as default RenderView size";
    size = display::Screen::GetScreen()->GetPrimaryDisplay().size();
  }
  return size;
}

void DistillerPageWebContents::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == &TargetRenderFrameHost()) {
    ExecuteJavaScript();
  }
}

void DistillerPageWebContents::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  if (render_frame_host->IsInPrimaryMainFrame()) {
    content::WebContentsObserver::Observe(nullptr);
    DCHECK(state_ == LOADING_PAGE || state_ == EXECUTING_JAVASCRIPT);
    state_ = PAGELOAD_FAILED;
    OnWebContentsDistillationDone(GURL(), base::TimeTicks(), base::Value());
  }
}

void DistillerPageWebContents::ExecuteJavaScript() {
  DCHECK_EQ(LOADING_PAGE, state_);
  state_ = EXECUTING_JAVASCRIPT;
  content::WebContentsObserver::Observe(nullptr);
  // Stop any pending navigation since the intent is to distill the current
  // page.
  source_page_handle_->web_contents()->Stop();
  DVLOG(1) << "Beginning distillation";
  RunIsolatedJavaScript(
      &TargetRenderFrameHost(), script_,
      base::BindOnce(&DistillerPageWebContents::OnWebContentsDistillationDone,
                     weak_factory_.GetWeakPtr(),
                     source_page_handle_->web_contents()->GetLastCommittedURL(),
                     base::TimeTicks::Now()));
}

void DistillerPageWebContents::OnWebContentsDistillationDone(
    const GURL& page_url,
    const base::TimeTicks& javascript_start,
    base::Value value) {
  DCHECK(state_ == IDLE || state_ == LOADING_PAGE ||  // TODO(nyquist): 493795.
         state_ == PAGELOAD_FAILED || state_ == EXECUTING_JAVASCRIPT);
  state_ = IDLE;

  if (!javascript_start.is_null()) {
    base::TimeDelta javascript_time = base::TimeTicks::Now() - javascript_start;
    DVLOG(1) << "DomDistiller.Time.RunJavaScript = " << javascript_time;
  }

  DistillerPage::OnDistillationDone(page_url, &value);
}

content::RenderFrameHost& DistillerPageWebContents::TargetRenderFrameHost() {
  // Distiller is invoked through an explicit user gesture. We don't have code
  // path for triggering this on non-primary pages.
  // We target the currently visible primary page's main document to run the
  // distiller script, thus `GetPrimaryPage().GetMainDocument()` usage is valid
  // here.
  return source_page_handle_->web_contents()
      ->GetPrimaryPage()
      .GetMainDocument();
}

}  // namespace dom_distiller
