// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_NET_ERROR_INFO_H_
#define COMPONENTS_ERROR_PAGE_COMMON_NET_ERROR_INFO_H_

namespace error_page {

// Network error page events.  Used for UMA statistics and its values must be
// mirrored in NetErrorPageEvents in enums.xml.
enum NetworkErrorPageEvent {
  NETWORK_ERROR_PAGE_SHOWN = 0,  // Error pages shown.

  NETWORK_ERROR_PAGE_RELOAD_BUTTON_SHOWN = 1,    // Reload buttons shown.
  NETWORK_ERROR_PAGE_RELOAD_BUTTON_CLICKED = 2,  // Reload button clicked.
  NETWORK_ERROR_PAGE_RELOAD_BUTTON_ERROR = 3,    // Reload button clicked
                                                 // -> error.

  // Obsolete values used for the "Show saved copy" button.
  // NETWORK_ERROR_PAGE_SHOW_SAVED_COPY_BUTTON_SHOWN = 4,
  // NETWORK_ERROR_PAGE_SHOW_SAVED_COPY_BUTTON_CLICKED = 5,
  // NETWORK_ERROR_PAGE_SHOW_SAVED_COPY_BUTTON_ERROR = 6,

  NETWORK_ERROR_PAGE_MORE_BUTTON_CLICKED = 7,  // More button clicked.

  NETWORK_ERROR_PAGE_BROWSER_INITIATED_RELOAD = 8,  // Reload from browser.

  // Obsolete values used for when "Show saved copy" and "Reload" buttons were
  // both shown.
  //
  // NETWORK_ERROR_PAGE_BOTH_BUTTONS_SHOWN = 9,
  // NETWORK_ERROR_PAGE_BOTH_BUTTONS_RELOAD_CLICKED = 10,
  // NETWORK_ERROR_PAGE_BOTH_BUTTONS_SHOWN_SAVED_COPY_CLICKED = 11,

  NETWORK_ERROR_EASTER_EGG_ACTIVATED = 12,  // Easter egg activated.

  // For "Google cached copy" button experiment.
  NETWORK_ERROR_PAGE_CACHED_COPY_BUTTON_SHOWN = 13,
  NETWORK_ERROR_PAGE_CACHED_COPY_BUTTON_CLICKED = 14,
  // Obsolete. No longer experimenting with the label.
  // NETWORK_ERROR_PAGE_CACHED_PAGE_BUTTON_SHOWN = 15,
  // NETWORK_ERROR_PAGE_CACHED_PAGE_BUTTON_CLICKED = 16,

  NETWORK_ERROR_DIAGNOSE_BUTTON_CLICKED = 17,  // Diagnose button clicked.

  // For the button to show all offline pages.
  // Obsolete. No longer showing this.
  // NETWORK_ERROR_PAGE_SHOW_OFFLINE_PAGES_BUTTON_SHOWN = 18,
  // NETWORK_ERROR_PAGE_SHOW_OFFLINE_PAGES_BUTTON_CLICKED = 19,

  // For the button to show offline copy.
  // Obsolete. No longer showing this.
  // NETWORK_ERROR_PAGE_SHOW_OFFLINE_COPY_BUTTON_SHOWN = 20,
  // NETWORK_ERROR_PAGE_SHOW_OFFLINE_COPY_BUTTON_CLICKED = 21,

  NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_SHOWN = 22,
  NETWORK_ERROR_PAGE_DOWNLOAD_BUTTON_CLICKED = 23,

  // Values for suggested content on the net-error page:

  // A list containing at least one item of offline content suggestions was
  // shown in the expanded/shown state.
  NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN = 24,
  // An item from the offline content suggestions list was clicked.
  NETWORK_ERROR_PAGE_OFFLINE_SUGGESTION_CLICKED = 25,
  // A link that opens the downloads page was clicked.
  NETWORK_ERROR_PAGE_OFFLINE_DOWNLOADS_PAGE_CLICKED = 26,
  // A summary of available offline content was shown.
  NETWORK_ERROR_PAGE_OFFLINE_CONTENT_SUMMARY_SHOWN = 27,
  // A list containing at least one item of offline content suggestions was
  // shown in the collapsed/hidden state.
  NETWORK_ERROR_PAGE_OFFLINE_SUGGESTIONS_SHOWN_COLLAPSED = 28,
  // The error page was shown because the device is offline (this is the dino
  // page).
  NETWORK_ERROR_PAGE_OFFLINE_ERROR_SHOWN = 29,

  NETWORK_ERROR_PAGE_EVENT_MAX,
};

// The status of a DNS probe.
//
// The DNS_PROBE_FINISHED_* values are used in histograms, so:
// 1. FINISHED_UNKNOWN must remain the first FINISHED_* value.
// 2. FINISHED_* values must not be rearranged relative to FINISHED_UNKNOWN.
// 3. New FINISHED_* values must be inserted at the end.
// 4. New non-FINISHED_* values cannot be inserted.
enum DnsProbeStatus {
  // A DNS probe may be run for this error page.  (This status is only used on
  // the renderer side before it's received a status update from the browser.)
  DNS_PROBE_POSSIBLE,

  // A DNS probe will not be run for this error page.  (This happens if the
  // user has the "Use web service to resolve navigation errors" preference
  // turned off, or if probes are disabled by the field trial.)
  DNS_PROBE_NOT_RUN,

  // A DNS probe has been started for this error page.  The renderer should
  // expect to receive another IPC with one of the FINISHED statuses once the
  // probe has finished (as long as the error page is still loaded).
  DNS_PROBE_STARTED,

  // A DNS probe has finished with one of the following results:

  // The probe was inconclusive.
  DNS_PROBE_FINISHED_INCONCLUSIVE,

  // There's no internet connection.
  DNS_PROBE_FINISHED_NO_INTERNET,

  // The DNS configuration is wrong, or the servers are down or broken.
  DNS_PROBE_FINISHED_BAD_CONFIG,

  // The DNS servers are working fine, so the domain must not exist.
  DNS_PROBE_FINISHED_NXDOMAIN,

  DNS_PROBE_MAX
};

// Returns a string representing |status|.  It should be simply the name of
// the value as a string, but don't rely on that.  This is presented to the
// user as part of the DNS error page (as the error code, at the bottom),
// and is also used in some verbose log messages.
//
// The function will NOTREACHED() and return an empty string if given an int
// that does not match a value in DnsProbeStatus (or if it is DNS_PROBE_MAX,
// which is not a real status).
const char* DnsProbeStatusToString(int status);

// Returns true if |status| is one of the DNS_PROBE_FINISHED_* statuses.
bool DnsProbeStatusIsFinished(DnsProbeStatus status);

// Record specific error page events.
void RecordEvent(NetworkErrorPageEvent event);

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_NET_ERROR_INFO_H_
