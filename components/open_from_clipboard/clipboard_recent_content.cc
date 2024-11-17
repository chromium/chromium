// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/open_from_clipboard/clipboard_recent_content.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/open_from_clipboard/clipboard_recent_content_features.h"
#include "components/variations/variations_associated_data.h"
#include "url/url_constants.h"

namespace {
ClipboardRecentContent* g_clipboard_recent_content = nullptr;

}  // namespace

ClipboardRecentContent::ClipboardRecentContent() = default;

ClipboardRecentContent::~ClipboardRecentContent() {
}

// static
ClipboardRecentContent* ClipboardRecentContent::GetInstance() {
  return g_clipboard_recent_content;
}

// static
void ClipboardRecentContent::SetInstance(
    std::unique_ptr<ClipboardRecentContent> new_instance) {
  delete g_clipboard_recent_content;
  g_clipboard_recent_content = new_instance.release();
}

// static
base::TimeDelta ClipboardRecentContent::MaximumAgeOfClipboard() {
#if BUILDFLAG(IS_ANDROID)
  int default_maximum_age = base::Minutes(3).InSeconds();
#else
  int default_maximum_age = base::Minutes(10).InSeconds();
#endif  // BUILDFLAG(IS_ANDROID)
  int value = base::GetFieldTrialParamByFeatureAsInt(
      kClipboardMaximumAge, kClipboardMaximumAgeParam, default_maximum_age);
  return base::Seconds(value);
}
