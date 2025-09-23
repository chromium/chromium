// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(OmniboxTabHelper);

namespace {

constexpr char kNavigationToPopupShownHistogramPrefix[] =
    "Omnibox.NavigationToPopupShown";
constexpr char kMainDocumentElementAvailableHistogramSuffix[] =
    "MainDocumentElementAvailable";
constexpr char kPrimaryPageChangedHistogramSuffix[] = "PrimaryPageChanged";
constexpr char kDomContentLoadedHistogramSuffix[] = "DomContentLoaded";
constexpr char kByPageContextHistogramPrefix[] = "ByPageContext";

void LogNavigationToPopupUma(std::string_view event_name,
                             std::string_view page_context,
                             base::TimeDelta time_to_log) {
  // Custom buckets from 1 millisecond to 1 minute, with 60 buckets in total.
  // Meaning each bucket is 1 second wide.
  base::UmaHistogramCustomTimes(
      base::StrCat({kNavigationToPopupShownHistogramPrefix, ".", event_name}),
      time_to_log, base::Milliseconds(0), base::Seconds(60), 60);
  base::UmaHistogramCustomTimes(
      base::StrCat({kNavigationToPopupShownHistogramPrefix, ".", event_name,
                    ".", kByPageContextHistogramPrefix, ".", page_context}),
      time_to_log, base::Milliseconds(0), base::Seconds(60), 60);
}

PaywallSignal ToPaywallSignal(std::optional<bool> paywall_signal) {
  if (paywall_signal.has_value()) {
    // If `paywall_signal` is available and true, it means the page is paywalled
    // and contextual suggestions should not be shown.
    return paywall_signal.value() ? PaywallSignal::kSignalPresent
                                  : PaywallSignal::kSignalNotPresent;
  }
  // Finally, if no signal was extracted from the page, then the signal is
  // unavailable due to missing page content.
  return PaywallSignal::kUnknown;
}

}  // namespace

OmniboxTabHelper::~OmniboxTabHelper() = default;
OmniboxTabHelper::OmniboxTabHelper(content::WebContents* contents,
                                   Profile* profile)
    : content::WebContentsUserData<OmniboxTabHelper>(*contents),
      content::WebContentsObserver(contents) {
  // Only fetch the entire APC if the paywall signal feature flag is enabled AND
  // the faster FrameMetadataObserver push service is disabled.
  if (omnibox_feature_configs::ContextualSearch::Get().use_apc_paywall_signal &&
      !base::FeatureList::IsEnabled(blink::features::kFrameMetadataObserver)) {
    if (auto* service = page_content_annotations::
            PageContentExtractionServiceFactory::GetForProfile(profile)) {
      // TODO(crbug.com/426665820): There are currently two ways the paywall
      // signal is being fetched. This uses the PageContentExtractionService
      // which takes a while to run, but has been around longer so is safer.
      // Eventually, once FrameMetadataObserver is well tested, this should be
      // cleaned up in favor of that.
      page_content_service_observation_.Observe(service);
    }
  }
}

void OmniboxTabHelper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxTabHelper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxTabHelper::OnInputStateChanged() {
  for (auto& observer : observers_) {
    observer.OnOmniboxInputStateChanged();
  }
}

void OmniboxTabHelper::OnInputInProgress(bool in_progress) {
  for (auto& observer : observers_) {
    observer.OnOmniboxInputInProgress(in_progress);
  }
}

void OmniboxTabHelper::OnFocusChanged(OmniboxFocusState state,
                                      OmniboxFocusChangeReason reason) {
  for (auto& observer : observers_) {
    observer.OnOmniboxFocusChanged(state, reason);
  }
}

void OmniboxTabHelper::OnPopupVisibilityChanged(
    bool popup_is_open,
    metrics::OmniboxEventProto::PageClassification page_classification) {
  for (auto& observer : observers_) {
    observer.OnOmniboxPopupVisibilityChanged(popup_is_open);
  }

  if (popup_is_open) {
    MaybeLogPaywallSignal();
    MaybeLogNavigationToPopupShownTimings(page_classification);
  }
}

std::optional<bool> OmniboxTabHelper::IsPagePaywalled() {
  return page_has_apc_paywall_signal_;
}

void OmniboxTabHelper::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  // Ignore if the APC does not belong to the primary page of this tabs web
  // contents.
  if (&page != &(GetWebContents().GetPrimaryPage())) {
    return;
  }
  page_has_apc_paywall_signal_ =
      page_content.has_main_frame_data() &&
      page_content.main_frame_data().has_paid_content_metadata() &&
      page_content.main_frame_data()
          .paid_content_metadata()
          .contains_paid_content();
}

void OmniboxTabHelper::PrimaryPageChanged(content::Page& page) {
  page_has_apc_paywall_signal_.reset();

  // Reset old times to avoid logging them incorrectly.
  primary_main_document_element_available_time_.reset();
  dom_content_loaded_time_.reset();
  logged_current_navigation_timings_ = false;

  primary_page_changed_time_ = base::ElapsedTimer();

  AddMetadataObserver(page);
}

void OmniboxTabHelper::AddMetadataObserver(content::Page& page) {
  if (!base::FeatureList::IsEnabled(blink::features::kFrameMetadataObserver) ||
      !omnibox_feature_configs::ContextualSearch::Get()
           .use_apc_paywall_signal) {
    return;
  }

  paid_content_metadata_observer_receiver_.reset();

  mojo::Remote<blink::mojom::FrameMetadataObserverRegistry>
      frame_metadata_observer_registry;
  auto& render_frame_host = page.GetMainDocument();
  render_frame_host.GetRemoteInterfaces()->GetInterface(
      frame_metadata_observer_registry.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<blink::mojom::PaidContentMetadataObserver> remote;
  paid_content_metadata_observer_receiver_.Bind(
      remote.InitWithNewPipeAndPassReceiver());

  frame_metadata_observer_registry->AddPaidContentMetadataObserver(
      std::move(remote));
}

void OmniboxTabHelper::PrimaryMainDocumentElementAvailable() {
  primary_main_document_element_available_time_ = base::ElapsedTimer();
}

void OmniboxTabHelper::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // Ignore events from subframes.
  if (render_frame_host->GetParent()) {
    return;
  }
  dom_content_loaded_time_ = base::ElapsedTimer();
}

void OmniboxTabHelper::MaybeLogNavigationToPopupShownTimings(
    metrics::OmniboxEventProto::PageClassification page_classification) {
  if (logged_current_navigation_timings_) {
    return;
  }
  logged_current_navigation_timings_ = true;

  // If the primary page hasn't changed, then there is nothing to log, and this
  // tab is probably on the NTP, so exit early to avoid skewing metrics.
  if (!primary_page_changed_time_.has_value()) {
    return;
  }

  const std::string page_context =
      metrics::OmniboxEventProto::PageClassification_Name(page_classification);

  LogNavigationToPopupUma(kPrimaryPageChangedHistogramSuffix, page_context,
                          primary_page_changed_time_->Elapsed());

  if (primary_main_document_element_available_time_.has_value()) {
    LogNavigationToPopupUma(
        kMainDocumentElementAvailableHistogramSuffix, page_context,
        primary_main_document_element_available_time_->Elapsed());
  }

  if (dom_content_loaded_time_.has_value()) {
    LogNavigationToPopupUma(kDomContentLoadedHistogramSuffix, page_context,
                            dom_content_loaded_time_->Elapsed());
  }
}

void OmniboxTabHelper::MaybeLogPaywallSignal() {
  // If the page content service is not observing, then the paywall signal is
  // unavailable to be fetched.
  if (!page_content_service_observation_.IsObserving() &&
      !paid_content_metadata_observer_receiver_.is_bound()) {
    return;
  }

  const auto paywall_signal = ToPaywallSignal(page_has_apc_paywall_signal_);
  base::UmaHistogramEnumeration("Omnibox.OnPopupOpen.PaywallSignal",
                                paywall_signal);
}

void OmniboxTabHelper::OnPaidContentMetadataChanged(bool has_paid_content) {
  page_has_apc_paywall_signal_ = has_paid_content;
}
