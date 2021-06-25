// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/app_source_url_recorder.h"

#include "base/atomic_sequence_num.h"
#include "components/crx_file/id_util.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

SourceId AssignNewAppId() {
  static base::AtomicSequenceNumber seq;
  return ConvertToSourceId(seq.GetNext() + 1, SourceIdType::APP_ID);
}

SourceId AppSourceUrlRecorder::GetSourceIdForChromeApp(
    const std::string& app_id) {
  DCHECK(!app_id.empty());
  GURL url("app://" + app_id);
  return GetSourceIdForUrl(url, AppType::kChromeApp);
}

SourceId AppSourceUrlRecorder::GetSourceIdForChromeExtension(
    const std::string& id) {
  GURL url("chrome-extension://" + id);
  return GetSourceIdForUrl(url, AppType::kExtension);
}

SourceId AppSourceUrlRecorder::GetSourceIdForArcPackageName(
    const std::string& package_name) {
  DCHECK(!package_name.empty());
  GURL url("app://" + package_name);
  return GetSourceIdForUrl(url, AppType::kArc);
}

SourceId AppSourceUrlRecorder::GetSourceIdForArc(
    const std::string& package_name) {
  const std::string package_name_hash =
      crx_file::id_util::GenerateId(package_name);
  GURL url("app://play/" + package_name_hash);
  return GetSourceIdForUrl(url, AppType::kArc);
}

SourceId AppSourceUrlRecorder::GetSourceIdForPWA(const GURL& url) {
  return GetSourceIdForUrl(url, AppType::kPWA);
}

SourceId AppSourceUrlRecorder::GetSourceIdForUrl(const GURL& url,
                                                 AppType app_type) {
  ukm::DelegatingUkmRecorder* const recorder =
      ukm::DelegatingUkmRecorder::Get();
  if (!recorder)
    return kInvalidSourceId;

  const SourceId source_id = AssignNewAppId();
  if (base::FeatureList::IsEnabled(kUkmAppLogging)) {
    recorder->UpdateAppURL(source_id, url, app_type);
  }
  return source_id;
}

}  // namespace ukm
