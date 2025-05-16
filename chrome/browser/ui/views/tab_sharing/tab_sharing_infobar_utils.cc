// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_infobar_utils.h"

#include "base/metrics/histogram_functions.h"

namespace {

// The histogram name must be kept in sync with the definition in
// tools/metrics/histograms/metadata/media/histograms.xml.
const char kTabSharingInfoBarInteractionHistogram[] =
    "Media.Ui.GetDisplayMedia.TabSharingInfoBarInteraction";

}  // namespace

void RecordUma(TabSharingInfoBarInteraction interaction) {
  base::UmaHistogramEnumeration(kTabSharingInfoBarInteractionHistogram,
                                interaction);
}
