// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_IWA_SOURCE_URL_RECORDER_H_
#define COMPONENTS_UKM_IWA_SOURCE_URL_RECORDER_H_

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace web_app {
class IsolatedWebAppMetricsHelper;
}

namespace ukm {

class IwaSourceUrlRecorder {
 private:
  friend class ::web_app::IsolatedWebAppMetricsHelper;

  static SourceId GetSourceIdForIwaUrl(const GURL& iwa_url);
  static void MarkSourceForDeletion(SourceId source_id);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_IWA_SOURCE_URL_RECORDER_H_
