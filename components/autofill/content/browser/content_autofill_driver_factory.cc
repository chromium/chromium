// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
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

std::unique_ptr<AutofillDriver> CreateDriver(
    content::RenderFrameHost* render_frame_host,
    AutofillClient* client,
    const std::string& app_locale,
    ContentAutofillRouter* router,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback) {
  return std::make_unique<ContentAutofillDriver>(
      render_frame_host, client, app_locale, router, enable_download_manager,
      std::move(autofill_manager_factory_callback));
}

}  // namespace

const char ContentAutofillDriverFactory::
    kContentAutofillDriverFactoryWebContentsUserDataKey[] =
        "web_contents_autofill_driver_factory";

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() = default;

// static
void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager) {
  CreateForWebContentsAndDelegate(
      contents, client, app_locale, enable_download_manager,
      AutofillManager::AutofillManagerFactoryCallback());
}

void ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
    content::WebContents* contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      kContentAutofillDriverFactoryWebContentsUserDataKey,
      std::make_unique<ContentAutofillDriverFactory>(
          contents, client, app_locale, enable_download_manager,
          std::move(autofill_manager_factory_callback)));
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
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // We try to bind to the driver of this render frame host,
  // but if driver is not ready for this render frame host for now,
  // the request will be just dropped, this would cause closing the message pipe
  // which would raise connection error to peer side.
  // Peer side could reconnect later when needed.
  if (!web_contents)
    return;

  ContentAutofillDriverFactory* factory =
      ContentAutofillDriverFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  ContentAutofillDriver* driver = factory->DriverForFrame(render_frame_host);
  if (driver)
    driver->BindPendingReceiver(std::move(pending_receiver));
}

ContentAutofillDriverFactory::ContentAutofillDriverFactory(
    content::WebContents* web_contents,
    AutofillClient* client,
    const std::string& app_locale,
    BrowserAutofillManager::AutofillDownloadManagerState
        enable_download_manager,
    AutofillManager::AutofillManagerFactoryCallback
        autofill_manager_factory_callback)
    : AutofillDriverFactory(client),
      content::WebContentsObserver(web_contents),
      app_locale_(app_locale),
      enable_download_manager_(enable_download_manager),
      autofill_manager_factory_callback_(
          std::move(autofill_manager_factory_callback)) {}

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  AutofillDriver* driver = DriverForKey(render_frame_host);

  // ContentAutofillDriver are created on demand here.
  if (!driver) {
    AddForKey(
        render_frame_host,
        base::BindRepeating(CreateDriver, render_frame_host, client(),
                            app_locale_, &router_, enable_download_manager_,
                            autofill_manager_factory_callback_));
    driver = DriverForKey(render_frame_host);
  }

  // This cast is safe because AutofillDriverFactory::AddForKey is protected
  // and always called with ContentAutofillDriver instances within
  // ContentAutofillDriverFactory.
  return static_cast<ContentAutofillDriver*>(driver);
}

void ContentAutofillDriverFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  ContentAutofillDriver* driver =
      static_cast<ContentAutofillDriver*>(DriverForKey(render_frame_host));
  if (driver) {
    if (render_frame_host->GetLifecycleState() !=
        content::RenderFrameHost::LifecycleState::kPrerendering) {
      driver->MaybeReportAutofillWebOTPMetrics();
    }

    // If the popup menu has been triggered from within an iframe and that
    // frame is deleted, hide the popup. This is necessary because the popup
    // may actually be shown by the AutofillExternalDelegate of an ancestor
    // frame, which is not notified about |render_frame_host|'s destruction
    // and therefore won't close the popup.
    if (render_frame_host->GetParent() &&
        router_.last_queried_source() == driver) {
      DCHECK_NE(content::RenderFrameHost::LifecycleState::kPrerendering,
                render_frame_host->GetLifecycleState());
      router_.HidePopup(driver);
    }
    if (!render_frame_host->GetParent()) {
      router_.Reset();
    } else {
      // UnregisterDriver() must not be called if |driver| belongs to the main
      // frame because of crbug/1190640.
      router_.UnregisterDriver(driver);
    }
  }
  DeleteForKey(render_frame_host);
}

void ContentAutofillDriverFactory::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(crbug/1117451): Clean up experiment code.
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
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
      DriverForFrame(render_frame_host)->ProbablyFormSubmitted();
    }
  }
}

void ContentAutofillDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      (navigation_handle->IsInMainFrame() ||
       navigation_handle->HasSubframeNavigationEntryCommitted())) {
    ContentAutofillDriver* driver =
        DriverForFrame(navigation_handle->GetRenderFrameHost());
    if (!navigation_handle->IsSameDocument() &&
        !navigation_handle->IsServedFromBackForwardCache()) {
      if (navigation_handle->IsInMainFrame()) {
        router_.Reset();
      } else {
        // UnregisterDriver() must not be called if |driver| belongs to the main
        // frame because of crbug/1190640.
        router_.UnregisterDriver(driver);
      }
    }
    NavigationFinished(AutofillDriverFactory::HideUi(
        !navigation_handle->IsInPrerenderedMainFrame()));
    driver->DidNavigateFrame(navigation_handle);
  }
}

void ContentAutofillDriverFactory::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    TabHidden();
}

void ContentAutofillDriverFactory::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* render_frame_host =
      navigation_handle->GetRenderFrameHost();
  content::GlobalRenderFrameHostId render_frame_host_id(
      render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID());
  // No need to report the metrics here if navigating to a different
  // RenderFrameHost. It will be reported in |RenderFrameDeleted|.
  // TODO(crbug.com/936696): Remove this logic when RenderDocument is enabled
  // everywhere.
  if (render_frame_host_id !=
      navigation_handle->GetPreviousRenderFrameHostId()) {
    return;
  }
  // Do not report metrics if prerendering.
  if (render_frame_host->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    return;
  }
  AutofillDriver* driver = DriverForFrame(render_frame_host);
  if (!driver)
    return;
  static_cast<ContentAutofillDriver*>(driver)
      ->MaybeReportAutofillWebOTPMetrics();
}

}  // namespace autofill
