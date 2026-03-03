// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_util.h"

#include "base/metrics/histogram_macros.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"

namespace media_message_center {

namespace {

// The maximum number of media notifications to count when recording the
// Media.Notification.Count histogram. 20 was chosen because it would be very
// unlikely to see a user with 20+ things playing at once.
const int kMediaNotificationCountHistogramMax = 20;

}  // namespace

const char kCountHistogramName[] = "Media.Notification.Count";
const char kCastCountHistogramName[] = "Media.Notification.Cast.Count";

bool IsOriginGoodForDisplay(const url::Origin& origin) {
  return !origin.opaque() ||
         origin.GetTupleOrPrecursorTupleIfOpaque().IsValid();
}

std::u16string GetOriginNameForDisplay(const url::Origin& origin) {
  const auto url = origin.opaque()
                       ? origin.GetTupleOrPrecursorTupleIfOpaque().GetURL()
                       : origin.GetURL();
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

void RecordConcurrentNotificationCount(size_t count) {
  UMA_HISTOGRAM_EXACT_LINEAR(kCountHistogramName, count,
                             kMediaNotificationCountHistogramMax);
}

void RecordConcurrentCastNotificationCount(size_t count) {
  UMA_HISTOGRAM_EXACT_LINEAR(kCastCountHistogramName, count,
                             kMediaNotificationCountHistogramMax);
}

}  // namespace media_message_center
