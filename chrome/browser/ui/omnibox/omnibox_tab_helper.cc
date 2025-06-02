// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"

#include "base/observer_list.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/common/omnibox_feature_configs.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(OmniboxTabHelper);

OmniboxTabHelper::~OmniboxTabHelper() = default;
OmniboxTabHelper::OmniboxTabHelper(content::WebContents* contents,
                                   Profile* profile)
    : content::WebContentsUserData<OmniboxTabHelper>(*contents),
      content::WebContentsObserver(contents) {
  // Only fetch the APC paywall signal if the feature flag is enabled.
  if (omnibox_feature_configs::ContextualSearch::Get().use_apc_paywall_signal) {
    if (auto* service = page_content_annotations::
            PageContentExtractionServiceFactory::GetForProfile(profile)) {
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

void OmniboxTabHelper::OnPopupVisibilityChanged(bool popup_is_open) {
  for (auto& observer : observers_) {
    observer.OnOmniboxPopupVisibilityChanged(popup_is_open);
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
}
