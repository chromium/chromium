// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/request_canceler.h"

class GURL;

namespace network {
class SimpleURLLoader;
}

namespace safe_browsing {

class PasswordProtectionServiceBase;

using password_manager::metrics_util::PasswordType;

// A request for checking if an unfamiliar login form or a password reuse event
// is safe. PasswordProtectionRequest objects are owned by
// PasswordProtectionServiceBase indicated by |password_protection_service_|.
// PasswordProtectionServiceBase is RefCountedThreadSafe such that it can post
// task safely between IO and UI threads. It can only be destroyed on UI thread.
//
// PasswordProtectionRequest flow:
// Step| Thread |                    Task
// (1) |   UI   | If incognito or !SBER, quit request.
// (2) |   UI   | Add task to IO thread for allowlist checking.
// (3) |   IO   | Check allowlist and return the result back to UI thread.
// (4) |   UI   | If allowlisted, check verdict cache; else quit request.
// (5) |   UI   | If verdict cached, quit request; else prepare request proto.
// (6) |   UI   | Collect features related to the DOM of the page.
// (7) |   UI   | If appropriate, compute visual features of the page.
// (7) |   UI   | Start a timeout task, and send network request.
// (8) |   UI   | On receiving response, handle response and finish.
//     |        | On request timeout, cancel request.
//     |        | On deletion of |password_protection_service_|, cancel request.
class PasswordProtectionRequest
    : public CancelableRequest,
      public base::RefCountedDeleteOnSequence<PasswordProtectionRequest> {
 public:
  // Not copyable or movable
  PasswordProtectionRequest(const PasswordProtectionRequest&) = delete;
  PasswordProtectionRequest& operator=(const PasswordProtectionRequest&) =
      delete;

  // Starts processing request by checking extended reporting and incognito
  // conditions.
  void Start();

  // CancelableRequest implementation
  void Cancel(bool timed_out) override;

  // Processes the received response.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  GURL main_frame_url() const { return main_frame_url_; }

  const LoginReputationClientRequest* request_proto() const {
    return request_proto_.get();
  }

  LoginReputationClientRequest::TriggerType trigger_type() const {
    return trigger_type_;
  }

  const std::string username() const { return username_; }

  PasswordType password_type() const { return password_type_; }

  const std::vector<std::string>& matching_domains() const {
    return matching_domains_;
  }

  const std::vector<password_manager::MatchingReusedCredential>&
  matching_reused_credentials() const {
    return matching_reused_credentials_;
  }

  bool is_modal_warning_showing() const { return is_modal_warning_showing_; }

  void set_is_modal_warning_showing(bool is_warning_showing) {
    is_modal_warning_showing_ = is_warning_showing;
  }

  RequestOutcome request_outcome() const { return request_outcome_; }

  void set_request_outcome(RequestOutcome request_outcome) {
    request_outcome_ = request_outcome;
  }

  void finish_for_testing(
      RequestOutcome outcome,
      std::unique_ptr<LoginReputationClientResponse> response) {
    Finish(outcome, std::move(response));
  }

  virtual base::WeakPtr<PasswordProtectionRequest> AsWeakPtr() = 0;

 protected:
  friend class base::RefCountedThreadSafe<PasswordProtectionRequest>;

  PasswordProtectionRequest(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      const GURL& main_frame_url,
      const GURL& password_form_action,
      const GURL& password_form_frame_url,
      const std::string& mime_type,
      const std::string& username,
      PasswordType password_type,
      const std::vector<password_manager::MatchingReusedCredential>&
          matching_reused_credentials,
      LoginReputationClientRequest::TriggerType type,
      bool password_field_exists,
      PasswordProtectionServiceBase* pps,
      int request_timeout_in_ms);

  ~PasswordProtectionRequest() override;

  // Initiates network request to Safe Browsing backend.
  void SendRequest();

  // Initiates network request to Safe Browsing backend with the given oauth2
  // access token.
  void SendRequestWithToken(const std::string& access_token);

  // Records an event for the result of the URL reputation lookup if the user
  // enters their password on a website.
  virtual void MaybeLogPasswordReuseLookupEvent(
      RequestOutcome outcome,
      const LoginReputationClientResponse* response) = 0;

  // Subclasses may override this method to add pings to the WebUI.
  virtual void MaybeAddPingToWebUI(const std::string& oauth_token) {}

  // Subclasses may override this method to add responses to the WebUI.
  virtual void MaybeAddResponseToWebUI(
      const LoginReputationClientResponse& response) {}

  // The PasswordProtectionServiceBase instance owns |this|.
  // Can only be accessed on UI thread.
  PasswordProtectionServiceBase* password_protection_service() {
    return password_protection_service_;
  }

  std::unique_ptr<LoginReputationClientRequest> request_proto_;

  // Used in tests to avoid dispatching a real request. Tests using this must
  // manually finish the request.
  bool prevent_initiating_url_loader_for_testing_ = false;

 private:
  friend base::RefCountedDeleteOnSequence<PasswordProtectionRequest>;
  friend base::DeleteHelper<PasswordProtectionRequest>;

  // Start checking the allowlist.
  void CheckAllowlist();

  // If |main_frame_url_| matches allowlist, call Finish() immediately;
  // otherwise call CheckCachedVerdicts().
  void OnAllowlistCheckDone(bool match_allowlist);

  // Looks up cached verdicts. If verdict is already cached, call SendRequest();
  // otherwise call Finish().
  void CheckCachedVerdicts();

  // Fill |request_proto_| with appropriate values.
  void FillRequestProto(bool is_sampled_ping);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Returns whether client side detection feature collection is available.
  virtual bool IsClientSideDetectionEnabled();

  // Extracts DOM features.
  virtual void GetDomFeatures() = 0;

  // Returns whether visual feature collection is available.
  virtual bool IsVisualFeaturesEnabled();

  // Extracts visual features, if the page meets certain privacy conditions.
  virtual void MaybeCollectVisualFeatures() = 0;
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_ANDROID)
  // Sets the referring app info.
  virtual void SetReferringAppInfo() = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Start a timer to cancel the request if it takes too long.
  void StartTimeout();

  // |this| will be destroyed after calling this function.
  void Finish(RequestOutcome outcome,
              std::unique_ptr<LoginReputationClientResponse> response);

  // PasswordProtectionRequest passes its |ui_task_runner| construction
  // parameter to its RefCountedDeleteOnSequence base class, which exposes its
  // passed-in task runner as owning_task_runner(). Expose that |ui_task_runner|
  // parameter internally as ui_task_runner() for clarity.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner() {
    return owning_task_runner();
  }

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // Main frame URL of the login form.
  const GURL main_frame_url_;

  // The action URL of the password form.
  const GURL password_form_action_;

  // Frame url of the detected password form.
  const GURL password_form_frame_url_;

  // The contents MIME type.
  const raw_ref<const std::string, DanglingUntriaged> mime_type_;

  // The username of the reused password hash. The username can be an email or
  // a username for a non-GAIA or saved-password reuse. No validation has been
  // done on it.
  const std::string username_;

  // Type of the reused password.
  const PasswordType password_type_;

  // Domains from the Password Manager that match this password.
  // Should be non-empty if |reused_password_type_| == SAVED_PASSWORD.
  // Otherwise, may or may not be empty.
  const std::vector<std::string> matching_domains_;

  // Signon_realms from the Password Manager that match this password.
  // Should be non-empty if |reused_password_type_| == SAVED_PASSWORD.
  // Otherwise, may or may not be empty.
  const std::vector<password_manager::MatchingReusedCredential>
      matching_reused_credentials_;

  // If this request is for unfamiliar login page or for a password reuse event.
  const LoginReputationClientRequest::TriggerType trigger_type_;

  // If there is a password field on the page.
  const bool password_field_exists_;

  // When request is sent.
  base::TimeTicks request_start_time_;

  // SimpleURLLoader instance for sending request and receiving response.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The PasswordProtectionServiceBase instance owns |this|.
  // Can only be accessed on UI thread.
  raw_ptr<PasswordProtectionServiceBase, DanglingUntriaged>
      password_protection_service_;

  // The outcome of the password protection request.
  RequestOutcome request_outcome_ = RequestOutcome::UNKNOWN;

  // If we haven't receive response after this period of time, we cancel this
  // request.
  const int request_timeout_in_ms_;

  // Needed for canceling tasks posted to different threads.
  base::CancelableTaskTracker tracker_;

  // Whether there is a modal warning triggered by this request.
  bool is_modal_warning_showing_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
