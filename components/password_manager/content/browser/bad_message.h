// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_

#include "components/autofill/core/common/form_data.h"

namespace content {
class RenderFrameHost;
}

namespace password_manager {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum class BadMessageReason {
  CPMD_BAD_ORIGIN_FORMS_PARSED_OBSOLETE = 1,    // obsolete
  CPMD_BAD_ORIGIN_FORMS_RENDERED_OBSOLETE = 2,  // obsolete
  CPMD_BAD_ORIGIN_FORM_SUBMITTED = 3,
  CPMD_BAD_ORIGIN_FOCUSED_PASSWORD_FORM_FOUND_OBSOLETE = 4,  // obsolete
  CPMD_BAD_ORIGIN_IN_PAGE_NAVIGATION_OBSOLETE = 5,           // obsolete
  CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED = 6,
  CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD = 7,
  CPMD_BAD_ORIGIN_SAVE_GENERATION_FIELD_DETECTED_BY_CLASSIFIER_OBSOLETE =
      8,                                                // obsolete
  CPMD_BAD_ORIGIN_UPON_USER_INPUT_CHANGE_OBSOLETE = 9,  // obsolete
  CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED = 10,
  CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP = 11,
  CPMD_BAD_ORIGIN_SHOW_PASSWORD_EDITING_POPUP_OBSOLETE = 12,    // obsolete
  CPMD_BAD_ORIGIN_GENERATION_AVAILABLE_FOR_FORM_OBSOLETE = 13,  // obsolete
  CPMD_BAD_ORIGIN_PRERENDERING = 14,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. ContentPasswordManagerDriver becomes CPMD) plus a unique
  // description of the reason. After making changes, you MUST update
  // histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

namespace bad_message {

// Returns true if a password form operation is allowed to be performed on the
// URL specified by `form_url`, in the specified `frame`.  In particular,
// renderer-side logic should prevent any password manager usage for about:blank
// as well as data URLs, so this function returns false for those URLs and kills
// the renderer, as it might be exploited. Used as part of
// `CheckChildProcessSecurityPolicyForURL()` below. That function should be used
// for checking URLs sent in IPCs from the renderer to perform additional
// validation on the URL, whereas this function can be used on URLs retrieved
// from trusted browser-side state, such as from the RenderFrameHost itself.
bool CheckForIllegalURL(content::RenderFrameHost* frame,
                        const GURL& form_url,
                        BadMessageReason reason);

// Returns true if the renderer for `frame` is allowed to perform an operation
// on a password form with the provided URL. This performs a security check
// using content::ChildProcessSecurityPolicy to make sure that `frame`'s process
// is allowed to access `form_url`, and also uses `CheckForIllegalURL()` to
// check for URLs that should be blocked on the renderer side, such as about:
// and data: URLs. If either check fails, terminates the renderer, as it might
// be exploited. This function should always be used to validate URLs that are
// sent in IPCs from the renderer.
bool CheckChildProcessSecurityPolicyForURL(content::RenderFrameHost* frame,
                                           const GURL& form_url,
                                           BadMessageReason reason);

// Returns true if frame is not prerendering (when password manager updates
// are disallowed). Kills the renderer if we are prerendering.
bool CheckFrameNotPrerendering(content::RenderFrameHost* frame);

}  // namespace bad_message
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
