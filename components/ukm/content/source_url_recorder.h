// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_
#define COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace ukm {

// Initializes recording of UKM source URLs for the given WebContents.
// Note: this method is idempotent.
void InitializeSourceUrlRecorderForWebContents(
    content::WebContents* web_contents);

}  // namespace ukm

#endif  // COMPONENTS_UKM_CONTENT_SOURCE_URL_RECORDER_H_
