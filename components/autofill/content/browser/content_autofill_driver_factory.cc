// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

namespace {

bool ShouldEnableHeavyFormDataScraping(const version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      return true;
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
    case version_info::Channel::UNKNOWN:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

void BrowserDriverInitHook(AutofillClient* client,
                           const std::string& app_locale,
                           ContentAutofillDriver* driver) {
  driver->set_autofill_manager(std::make_unique<BrowserAutofillManager>(
      driver, client, app_locale,
      AutofillManager::EnableDownloadManager(true)));
  if (client && ShouldEnableHeavyFormDataScraping(client->GetChannel()))
    driver->GetAutofillAgent()->EnableHeavyFormDataScraping();
}

const char ContentAutofillDriverFactory::
    kContentAutofillDriverFactoryWebContentsUserDataKey[] =
        "web_contents_autofill_driver_factory";

// static
void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    DriverInitCallback driver_init_hook) {
  if (FromWebContents(contents))
    return;
  contents->SetUserData(kContentAutofillDriverFactoryWebContentsUserDataKey,
                        base::WrapUnique(new ContentAutofillDriverFactory(
                            contents, client, std::move(driver_init_hook))));
}

// static
ContentAutofillDriverFactory* ContentAutofillDriverFactory::FromWebContents(
    content::WebContents* contents) {
  return static_cast<ContentAutofillDriverFactory*>(contents->GetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey));
}

// static
void ContentAutofillDriverFactory::BindAutofillDriver(
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  ContentAutofillDriverFactory* factory = FromWebContents(web_contents);
  if (!factory) {
    // The message pipe will be closed and raise a connection error to peer
    // side. The peer side can reconnect later when needed.
    return;
  }

  if (auto* driver = factory->DriverForFrame(render_frame_host))
    driver->BindPendingReceiver(std::move(pending_receiver));
}

ContentAutofillDriverFactory::ContentAutofillDriverFactory(
    content::WebContents* web_contents,
    AutofillClient* client,
    DriverInitCallback driver_init_hook)
    : content::WebContentsObserver(web_contents),
      client_(client),
      driver_init_hook_(std::move(driver_init_hook)) {}

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() = default;

std::unique_ptr<ContentAutofillDriver>
ContentAutofillDriverFactory::CreateDriver(content::RenderFrameHost* rfh) {
  auto driver = std::make_unique<ContentAutofillDriver>(rfh, &router_);
  driver_init_hook_.Run(driver.get());
  return driver;
}

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  // Within fenced frames and their descendants, Password Manager should for now
  // be disabled (crbug.com/1294378).
  if (render_frame_host->IsNestedWithinFencedFrame() &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableWithinFencedFrame)) {
    return nullptr;
  }

  auto [iter, insertion_happened] =
      driver_map_.emplace(render_frame_host, nullptr);
  std::unique_ptr<ContentAutofillDriver>& driver = iter->second;
  if (insertion_happened) {
    // The `render_frame_host` may already be deleted (or be in the process of
    // being deleted). In this case, we must not create a new driver. Otherwise,
    // a driver might hold a deallocated RFH.
    //
    // For example, `render_frame_host` is deleted in the following sequence:
    // 1. `render_frame_host->~RenderFrameHostImpl()` starts and marks
    //    `render_frame_host` as deleted.
    // 2. `ContentAutofillDriverFactory::RenderFrameDeleted(render_frame_host)`
    //    destroys the driver of `render_frame_host`.
    // 3. `SomeOtherWebContentsObserver::RenderFrameDeleted(render_frame_host)`
    //    calls `DriverForFrame(render_frame_host)`.
    // 5. `render_frame_host->~RenderFrameHostImpl()` finishes.
    if (render_frame_host->IsRenderFrameLive()) {
      driver = CreateDriver(render_frame_host);
      DCHECK_EQ(driver_map_.find(render_frame_host)->second.get(),
                driver.get());
    } else {
      driver_map_.erase(iter);
      DCHECK_EQ(driver_map_.count(render_frame_host), 0u);
      return nullptr;
    }
  }
  DCHECK(driver.get());
  return driver.get();
}

void ContentAutofillDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = driver_map_.find(render_frame_host);
  if (it == driver_map_.end())
    return;

  ContentAutofillDriver* driver = it->second.get();
  DCHECK(driver);

  if (!render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering) &&
      driver->autofill_manager()) {
    driver->autofill_manager()->ReportAutofillWebOTPMetrics(
        render_frame_host->DocumentUsedWebOTP());
  }

  // If the popup menu has been triggered from within an iframe and that
  // frame is deleted, hide the popup. This is necessary because the popup
  // may actually be shown by the AutofillExternalDelegate of an ancestor
  // frame, which is not notified about |render_frame_host|'s destruction
  // and therefore won't close the popup.
  bool is_iframe = !driver->IsInAnyMainFrame();
  if (is_iframe && router_.last_queried_source() == driver) {
    DCHECK(!render_frame_host->IsInLifecycleState(
        content::RenderFrameHost::LifecycleState::kPrerendering));
    driver->renderer_events().HidePopup();
  }

  driver_map_.erase(it);
}

void ContentAutofillDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug/1117451): Clean up experiment code.
  if (base::FeatureList::IsEnabled(
          features::kAutofillProbableFormSubmissionInBrowser) &&
      navigation_handle->IsRendererInitiated() &&
      !navigation_handle->WasInitiatedByLinkClick() &&
      navigation_handle->IsInPrimaryMainFrame()) {
    content::GlobalRenderFrameHostId id =
        navigation_handle->GetPreviousRenderFrameHostId();
    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(id);
    if (render_frame_host) {
      if (auto* driver = DriverForFrame(render_frame_host))
        driver->ProbablyFormSubmitted({});
    }
  }
}

void ContentAutofillDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      (navigation_handle->IsInMainFrame() ||
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    if (auto* driver =
            DriverForFrame(navigation_handle->GetRenderFrameHost())) {
      if (!navigation_handle->IsInPrerenderedMainFrame()) {
        client_->HideAutofillPopup(PopupHidingReason::kNavigation);
        if (client_->IsTouchToFillCreditCardSupported())
          client_->HideTouchToFillCreditCard();
      }
      driver->DidNavigateFrame(navigation_handle);
    }
  }
}

void ContentAutofillDriverFactory::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    client_->HideAutofillPopup(PopupHidingReason::kTabGone);
    if (client_->IsTouchToFillCreditCardSupported())
      client_->HideTouchToFillCreditCard();
  }
}

}  // namespace autofill
