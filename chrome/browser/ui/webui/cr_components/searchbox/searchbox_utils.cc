// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"

#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

using OEP = ::metrics::OmniboxEventProto;

omnibox::ChromeAimEntryPoint PageClassificationToAimEntryPoint(
    metrics::OmniboxEventProto::PageClassification page_class) {
  switch (page_class) {
    // Omnibox Entry Points.
    case OEP::NTP_OMNIBOX_COMPOSEBOX:
      return omnibox::DESKTOP_CHROME_NTP_OMNIBOX_COMPOSEBOX_ENTRY_POINT;
    case OEP::SRP_OMNIBOX_COMPOSEBOX:
      return omnibox::DESKTOP_CHROME_SRP_OMNIBOX_COMPOSEBOX_ENTRY_POINT;
    case OEP::OTHER_OMNIBOX_COMPOSEBOX:
      return omnibox::DESKTOP_CHROME_OTHER_OMNIBOX_COMPOSEBOX_ENTRY_POINT;
    // Realbox Entry Point.
    case OEP::NTP_COMPOSEBOX:
      return omnibox::DESKTOP_CHROME_NTP_REALBOX_ENTRY_POINT;
    // Lens Entry Point.
    case OEP::CONTEXTUAL_SEARCHBOX:
      return omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT;
    // Co-browsing Entry Point.
    case OEP::CO_BROWSING_COMPOSEBOX:
      return omnibox::DESKTOP_CHROME_CO_BROWSING_COMPOSEBOX_ENTRY_POINT;
    default:
      return omnibox::UNKNOWN_AIM_ENTRY_POINT;
  }
}
