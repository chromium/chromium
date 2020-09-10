// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
#define COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_

#include <vector>
#include "components/autofill/core/common/form_data.h"

namespace autofill {
struct PasswordForm;
}

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
  CPMD_BAD_ORIGIN_FORMS_PARSED = 1,
  CPMD_BAD_ORIGIN_FORMS_RENDERED = 2,
  CPMD_BAD_ORIGIN_FORM_SUBMITTED = 3,
  CPMD_BAD_ORIGIN_FOCUSED_PASSWORD_FORM_FOUND_OBSOLETE = 4,  // obsolete
  CPMD_BAD_ORIGIN_IN_PAGE_NAVIGATION = 5,
  CPMD_BAD_ORIGIN_PASSWORD_NO_LONGER_GENERATED = 6,
  CPMD_BAD_ORIGIN_PRESAVE_GENERATED_PASSWORD = 7,
  CPMD_BAD_ORIGIN_SAVE_GENERATION_FIELD_DETECTED_BY_CLASSIFIER = 8,
  CPMD_BAD_ORIGIN_UPON_USER_INPUT_CHANGE = 9,
  CPMD_BAD_ORIGIN_AUTOMATIC_GENERATION_STATUS_CHANGED = 10,
  CPMD_BAD_ORIGIN_SHOW_MANUAL_PASSWORD_GENERATION_POPUP = 11,
  CPMD_BAD_ORIGIN_SHOW_PASSWORD_EDITING_POPUP = 12,
  CPMD_BAD_ORIGIN_GENERATION_AVAILABLE_FOR_FORM = 13,

  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. ContentPasswordManagerDriver becomes CPMD) plus a unique
  // description of the reason. After making changes, you MUST update
  // histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

namespace bad_message {
// Returns true if the renderer for |frame| is allowed to perform an operation
// on password form with given URL. Renderer-side logic should prevent any
// password manager usage for about:blank frames as well as data URLs.
// If that's not the case, kill the renderer, as it might be exploited.
bool CheckChildProcessSecurityPolicyForURL(content::RenderFrameHost* frame,
                                           const GURL& form_url,
                                           BadMessageReason reason);

// Returns true if the renderer for |frame| is allowed to perform an operation
// on |password_form|. If the origin mismatches, the process for |frame| is
// terminated and the function returns false.
// TODO: Delete this signature after transferring all driver calls to FormData
bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const autofill::PasswordForm& password_form,
    BadMessageReason reason);

// Same as above but checks every form in |forms|.
// TODO: Delete this signature after transferring all driver calls to FormData
bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const std::vector<autofill::PasswordForm>& forms,
    BadMessageReason reason);

bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const std::vector<autofill::FormData>& forms_data,
    BadMessageReason reason);

}  // namespace bad_message
}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CONTENT_BROWSER_BAD_MESSAGE_H_
