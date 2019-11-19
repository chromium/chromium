// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/syslog_logging.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace password_manager {
namespace bad_message {
namespace {

// Called when the browser receives a bad IPC message from a renderer process on
// the UI thread. Logs the event, records a histogram metric for the |reason|,
// and terminates the process for |host|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  LOG(ERROR)
      << "Terminating renderer for bad PasswordManager IPC message, reason "
      << static_cast<int>(reason);
  base::UmaHistogramSparse("Stability.BadMessageTerminated.PasswordManager",
                           static_cast<int>(reason));
  host->ShutdownForBadMessage(
      content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

bool CheckChildProcessSecurityPolicyForURL(content::RenderFrameHost* frame,
                                           const GURL& url,
                                           BadMessageReason reason) {
  // Renderer-side logic should prevent any password manager usage for
  // about:blank frames as well as data URLs.  If that's not the case, kill the
  // renderer, as it might be exploited.
  if (url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kDataScheme)) {
    SYSLOG(WARNING) << "Killing renderer: illegal password access from about: "
                    << "or data: URL. Reason: " << static_cast<int>(reason);
    bad_message::ReceivedBadMessage(frame->GetProcess(), reason);
    return false;
  }

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanAccessDataForOrigin(frame->GetProcess()->GetID(), url)) {
    SYSLOG(WARNING) << "Killing renderer: illegal password access. Reason: "
                    << static_cast<int>(reason);
    bad_message::ReceivedBadMessage(frame->GetProcess(), reason);
    return false;
  }

  return true;
}

}  // namespace

bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const autofill::PasswordForm& password_form,
    BadMessageReason reason) {
  return CheckChildProcessSecurityPolicyForURL(frame, password_form.origin,
                                               reason) &&
         CheckChildProcessSecurityPolicyForURL(
             frame, GURL(password_form.signon_realm), reason) &&
         CheckChildProcessSecurityPolicyForURL(
             frame, password_form.form_data.url, reason);
}

bool CheckChildProcessSecurityPolicy(
    content::RenderFrameHost* frame,
    const std::vector<autofill::PasswordForm>& forms,
    BadMessageReason reason) {
  for (const auto& form : forms) {
    if (!bad_message::CheckChildProcessSecurityPolicy(frame, form, reason))
      return false;
  }
  return true;
}

}  // namespace bad_message
}  // namespace password_manager
