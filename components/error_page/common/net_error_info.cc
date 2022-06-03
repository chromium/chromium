// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/net_error_info.h"

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace error_page {

const char* DnsProbeStatusToString(int status) {
  switch (status) {
    case DNS_PROBE_POSSIBLE:
      return "DNS_PROBE_POSSIBLE";
    case DNS_PROBE_NOT_RUN:
      return "DNS_PROBE_NOT_RUN";
    case DNS_PROBE_STARTED:
      return "DNS_PROBE_STARTED";
    case DNS_PROBE_FINISHED_INCONCLUSIVE:
      return "DNS_PROBE_FINISHED_INCONCLUSIVE";
    case DNS_PROBE_FINISHED_NO_INTERNET:
      return "DNS_PROBE_FINISHED_NO_INTERNET";
    case DNS_PROBE_FINISHED_BAD_CONFIG:
      return "DNS_PROBE_FINISHED_BAD_CONFIG";
    case DNS_PROBE_FINISHED_NXDOMAIN:
      return "DNS_PROBE_FINISHED_NXDOMAIN";
    case DNS_PROBE_FINISHED_BAD_SECURE_CONFIG:
      return "DNS_PROBE_FINISHED_BAD_SECURE_CONFIG";
    default:
      NOTREACHED();
      return "";
  }
}

bool DnsProbeStatusIsFinished(DnsProbeStatus status) {
  return status >= DNS_PROBE_FINISHED_INCONCLUSIVE && status < DNS_PROBE_MAX;
}

void RecordEvent(NetworkErrorPageEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Net.ErrorPageCounts", event,
                            NETWORK_ERROR_PAGE_EVENT_MAX);
}

}  // namespace error_page
