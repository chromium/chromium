// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_driver_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"

namespace autofill {

class ScopedAutofillManagersObservation;

void ContentAutofillDriverFactory::Observer::OnAutofillDriverFactoryDestroyed(
    AutofillDriverFactory& factory) {
  OnContentAutofillDriverFactoryDestroyed(
      static_cast<ContentAutofillDriverFactory&>(factory));
}

void ContentAutofillDriverFactory::Observer::OnAutofillDriverCreated(
    AutofillDriverFactory& factory,
    AutofillDriver& driver) {
  OnContentAutofillDriverCreated(
      static_cast<ContentAutofillDriverFactory&>(factory),
      static_cast<ContentAutofillDriver&>(driver));
}

void ContentAutofillDriverFactory::Observer::OnAutofillDriverStateChanged(
    AutofillDriverFactory& factory,
    AutofillDriver& driver,
    LifecycleState old_state,
    LifecycleState new_state) {
  OnContentAutofillDriverStateChanged(
      static_cast<ContentAutofillDriverFactory&>(factory),
      static_cast<ContentAutofillDriver&>(driver), old_state, new_state);
}

// static
ContentAutofillDriverFactory* ContentAutofillDriverFactory::FromWebContents(
    content::WebContents* contents) {
  ContentAutofillClient* client =
      ContentAutofillClient::FromWebContents(contents);
  if (!client) {
    return nullptr;
  }
  return &client->GetAutofillDriverFactory();
}

// static
void ContentAutofillDriverFactory::BindAutofillDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingAssociatedReceiver<mojom::AutofillDriver> pending_receiver) {
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
    ContentAutofillClient* client)
    : content::WebContentsObserver(web_contents), client_(*client) {}

ContentAutofillDriverFactory::~ContentAutofillDriverFactory() {
  for (auto& observer : observers()) {
    observer.OnAutofillDriverFactoryDestroyed(*this);
  }
  base::UmaHistogramCounts1000("Autofill.NumberOfDriversPerFactory",
                               max_drivers_);
}

ContentAutofillDriver* ContentAutofillDriverFactory::DriverForFrame(
    content::RenderFrameHost* render_frame_host) {
  // Within fenced frames and their descendants, Password Manager should for now
  // be disabled (crbug.com/1294378).
  if (render_frame_host->IsNestedWithinFencedFrame() &&
      !base::FeatureList::IsEnabled(blink::features::kFencedFramesAPIChanges)) {
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
    if (!render_frame_host->IsRenderFrameLive()) {
      driver_map_.erase(iter);
      DCHECK_EQ(driver_map_.count(render_frame_host), 0u);
      return nullptr;
    }
    driver = std::make_unique<ContentAutofillDriver>(render_frame_host, this);
    DCHECK_EQ(driver->GetLifecycleState(), LifecycleState::kInactive);
    for (auto& observer : observers()) {
      observer.OnAutofillDriverCreated(*this, *driver);
    }
    // TODO: crbug.com/342132628 - `driver->IsActive()` is guaranteed once
    // prerendered CADs are deferred.
    SetLifecycleStateAndNotifyObservers(
        *driver, driver->IsActive() ? LifecycleState::kActive
                                    : LifecycleState::kInactive);
    DCHECK_EQ(&driver_map_[render_frame_host], &driver);
  }
  DCHECK(driver.get());
  max_drivers_ = std::max(max_drivers_, driver_map_.size());
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
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // TODO: crbug.com/354043809 - Move out of CADF.
    driver->GetAutofillManager().ReportAutofillWebOTPMetrics(
        render_frame_host->DocumentUsedWebOTP());
  }

  SetLifecycleStateAndNotifyObservers(*driver,
                                      LifecycleState::kPendingDeletion);
  driver_map_.erase(it);
}

void ContentAutofillDriverFactory::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  auto* driver = DriverForFrame(render_frame_host);
  if (!driver) {
    return;
  }
  auto state =
      [new_state]() -> std::optional<ContentAutofillDriver::LifecycleState> {
    using RFH = content::RenderFrameHost::LifecycleState;
    using CAD = ContentAutofillDriver::LifecycleState;
    switch (new_state) {
      case RFH::kPendingCommit:  // Handled in DidFinishNavigation().
        return std::nullopt;

      case RFH::kActive:  // Potentially redundant with DriverForFrame().
        return CAD::kActive;

      case RFH::kPrerendering:
        // TODO: crbug.com/342132628 - Unreachable once prerendered CADs are
        // deferred.
      case RFH::kInBackForwardCache:
        return CAD::kInactive;

      case RFH::kPendingDeletion:  // Handled in RenderFrameDeleted().
        return std::nullopt;
    }
    NOTREACHED();
  }();
  if (state) {
    SetLifecycleStateAndNotifyObservers(*driver, *state);
  }
}

void ContentAutofillDriverFactory::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  auto* driver = DriverForFrame(navigation_handle->GetRenderFrameHost());
  if (!driver) {
    return;
  }

  if (navigation_handle->IsInPrimaryMainFrame() &&
      client().GetPaymentsAutofillClient() &&
      client().GetPaymentsAutofillClient()->GetAutofillOfferManager()) {
    // If the navigation happened in the main frame and the AutofillOfferManager
    // exists (not in Incognito windows, not in WebView), notify it about the
    // navigation event.
    // TODO: crbug.com/40178290 - Move out of CADF. Perhaps use the
    // LifecycleState changes to recognize navigations.
    client()
        .GetPaymentsAutofillClient()
        ->GetAutofillOfferManager()
        ->OnDidNavigateFrame(client());
  }

  // If the navigation is served from BFCache, then the pre-navigation RFH is
  // swapped with a post-navigation RFH, along with their associated CADs. We do
  // not reset the swapped-in CAD's state so that its state continues where we
  // left off when the CAD was swapped out. Similarly for prerendering.
  if (navigation_handle->IsServedFromBackForwardCache() ||
      navigation_handle->IsPrerenderedPageActivation()) {
    return;
  }

  SetLifecycleStateAndNotifyObservers(*driver, LifecycleState::kPendingReset);
  driver->Reset(/*pass_key=*/{});
  // TODO: crbug.com/342132628 - `driver->IsActive()` is guaranteed once
  // prerendered CADs are deferred.
  SetLifecycleStateAndNotifyObservers(
      *driver, driver->IsActive() ? AutofillDriver::LifecycleState::kActive
                                  : AutofillDriver::LifecycleState::kInactive);
}

std::vector<ContentAutofillDriver*>
ContentAutofillDriverFactory::GetExistingDrivers(
    base::PassKey<ScopedAutofillManagersObservation>) {
  std::vector<ContentAutofillDriver*> drivers;
  drivers.reserve(driver_map_.size());
  for (const auto& [rfh, driver] : driver_map_) {
    drivers.push_back(driver.get());
  }
  return drivers;
}

}  // namespace autofill
