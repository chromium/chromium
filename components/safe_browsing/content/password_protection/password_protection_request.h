// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/content/password_protection/password_protection_service.h"
#include "components/safe_browsing/core/password_protection/metrics_util.h"
#include "components/safe_browsing/core/password_protection/request_canceler.h"
#include "components/safe_browsing/core/proto/csd.pb.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if defined(OS_IOS)
// TODO(crbug.com/1147967): Enable in iOS once this file is moved to /core.
#else
#include "content/public/browser/browser_thread.h"
#endif  // defined(OS_IOS)

class GURL;

namespace network {
class SimpleURLLoader;
}

namespace content {
class WebContents;
}

namespace safe_browsing {

class PasswordProtectionNavigationThrottle;

using password_manager::metrics_util::PasswordType;

#if defined(OS_IOS)
using DeleteOnUIThread = web::WebThread::DeleteOnThread<web::WebThread::UI>;
#else
using DeleteOnUIThread =
    content::BrowserThread::DeleteOnThread<content::BrowserThread::UI>;
#endif  // defined(OS_IOS)

// A request for checking if an unfamiliar login form or a password reuse event
// is safe. PasswordProtectionRequest objects are owned by
// PasswordProtectionServiceBase indicated by |password_protection_service_|.
// PasswordProtectionServiceBase is RefCountedThreadSafe such that it can post
// task safely between IO and UI threads. It can only be destroyed on UI thread.
//
// PasswordProtectionRequest flow:
// Step| Thread |                    Task
// (1) |   UI   | If incognito or !SBER, quit request.
// (2) |   UI   | Add task to IO thread for whitelist checking.
// (3) |   IO   | Check whitelist and return the result back to UI thread.
// (4) |   UI   | If whitelisted, check verdict cache; else quit request.
// (5) |   UI   | If verdict cached, quit request; else prepare request proto.
// (6) |   UI   | Collect features related to the DOM of the page.
// (7) |   UI   | If appropriate, compute visual features of the page.
// (7) |   UI   | Start a timeout task, and send network request.
// (8) |   UI   | On receiving response, handle response and finish.
//     |        | On request timeout, cancel request.
//     |        | On deletion of |password_protection_service_|, cancel request.
class PasswordProtectionRequest
    : public CancelableRequest,
      public base::RefCountedThreadSafe<PasswordProtectionRequest,
                                        DeleteOnUIThread> {
 public:
  PasswordProtectionRequest(
      content::WebContents* web_contents,
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

  // Not copyable or movable
  PasswordProtectionRequest(const PasswordProtectionRequest&) = delete;
  PasswordProtectionRequest& operator=(const PasswordProtectionRequest&) =
      delete;

  base::WeakPtr<PasswordProtectionRequest> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

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

  content::WebContents* web_contents() const { return web_contents_; }

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

  // Keeps track of created navigation throttle.
  void AddThrottle(PasswordProtectionNavigationThrottle* throttle) {
    throttles_.insert(throttle);
  }

  void RemoveThrottle(PasswordProtectionNavigationThrottle* throttle) {
    throttles_.erase(throttle);
  }

  // Cancels navigation if there is modal warning showing, resumes it otherwise.
  void HandleDeferredNavigations();

 protected:
  friend class base::RefCountedThreadSafe<PasswordProtectionRequest>;

 private:
  friend DeleteOnUIThread;
  friend class base::DeleteHelper<PasswordProtectionRequest>;
  friend class PasswordProtectionServiceTest;
  friend class ChromePasswordProtectionServiceTest;
  ~PasswordProtectionRequest() override;

  // Start checking the whitelist.
  void CheckWhitelist();

  static void OnWhitelistCheckDoneOnIO(
      base::WeakPtr<PasswordProtectionRequest> weak_request,
      bool match_whitelist);

  // If |main_frame_url_| matches whitelist, call Finish() immediately;
  // otherwise call CheckCachedVerdicts().
  void OnWhitelistCheckDone(bool match_whitelist);

  // Looks up cached verdicts. If verdict is already cached, call SendRequest();
  // otherwise call Finish().
  void CheckCachedVerdicts();

  // Fill |request_proto_| with appropriate values.
  void FillRequestProto(bool is_sampled_ping);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Extracts DOM features.
  void GetDomFeatures();

  // Called when the DOM feature extraction is complete.
  void OnGetDomFeatures(mojom::PhishingDetectorResult result,
                        const std::string& verdict);

  // Called when the DOM feature extraction times out.
  void OnGetDomFeatureTimeout();

  // If appropriate, collects visual features, otherwise continues on to sending
  // the request.
  void MaybeCollectVisualFeatures();
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Collects visual features from the current login page.
  void CollectVisualFeatures();

  // Processes the screenshot of the login page into visual features.
  void OnScreenshotTaken(const SkBitmap& bitmap);

  // Called when the visual feature extraction is complete.
  void OnVisualFeatureCollectionDone(
      std::unique_ptr<VisualFeatures> visual_features);
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

  // Initiates network request to Safe Browsing backend.
  void SendRequest();

  // Start a timer to cancel the request if it takes too long.
  void StartTimeout();

  // |this| will be destroyed after calling this function.
  void Finish(RequestOutcome outcome,
              std::unique_ptr<LoginReputationClientResponse> response);

  // WebContents of the password protection event.
  content::WebContents* web_contents_;

  // Main frame URL of the login form.
  const GURL main_frame_url_;

  // The action URL of the password form.
  const GURL password_form_action_;

  // Frame url of the detected password form.
  const GURL password_form_frame_url_;

  // The contents MIME type.
  const std::string& mime_type_;

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
  PasswordProtectionServiceBase* password_protection_service_;

  // The outcome of the password protection request.
  RequestOutcome request_outcome_;

  // If we haven't receive response after this period of time, we cancel this
  // request.
  const int request_timeout_in_ms_;

  std::unique_ptr<LoginReputationClientRequest> request_proto_;

  // Needed for canceling tasks posted to different threads.
  base::CancelableTaskTracker tracker_;

  // Navigation throttles created for this |web_contents_| during |this|'s
  // lifetime. These throttles are owned by their corresponding
  // NavigationHandler instances.
  std::set<PasswordProtectionNavigationThrottle*> throttles_;

  // Whether there is a modal warning triggered by this request.
  bool is_modal_warning_showing_;

  // If a request is sent, this is the token returned by the WebUI.
  int web_ui_token_;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // When we start extracting visual features.
  base::TimeTicks visual_feature_start_time_;

  // The Mojo pipe used for extracting DOM features from the renderer.
  mojo::Remote<safe_browsing::mojom::PhishingDetector> phishing_detector_;

  // When we start extracting DOM features. Used to compute the duration of DOM
  // feature extraction, which is logged at
  // PasswordProtection.DomFeatureExtractionDuration.
  base::TimeTicks dom_feature_start_time_;

  // Whether the DOM features collection is finished, either by timeout or by
  // successfully gathering the features.
  bool dom_features_collection_complete_;
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

  // Cancels the request when it is no longer valid.
  std::unique_ptr<RequestCanceler> request_canceler_;

  base::WeakPtrFactory<PasswordProtectionRequest> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
