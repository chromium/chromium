// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
#define COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/safe_browsing/password_protection/metrics_util.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"

class GURL;

namespace network {
class SimpleURLLoader;
}

namespace safe_browsing {

class PasswordProtectionNavigationThrottle;

using password_manager::metrics_util::PasswordType;

// A request for checking if an unfamiliar login form or a password reuse event
// is safe. PasswordProtectionRequest objects are owned by
// PasswordProtectionService indicated by |password_protection_service_|.
// PasswordProtectionService is RefCountedThreadSafe such that it can post task
// safely between IO and UI threads. It can only be destroyed on UI thread.
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
class PasswordProtectionRequest : public base::RefCountedThreadSafe<
                                      PasswordProtectionRequest,
                                      content::BrowserThread::DeleteOnUIThread>,
                                  public content::WebContentsObserver {
 public:
  PasswordProtectionRequest(content::WebContents* web_contents,
                            const GURL& main_frame_url,
                            const GURL& password_form_action,
                            const GURL& password_form_frame_url,
                            const std::string& username,
                            PasswordType password_type,
                            const std::vector<std::string>& matching_origins,
                            LoginReputationClientRequest::TriggerType type,
                            bool password_field_exists,
                            PasswordProtectionService* pps,
                            int request_timeout_in_ms);

  base::WeakPtr<PasswordProtectionRequest> GetWeakPtr() {
    return weakptr_factory_.GetWeakPtr();
  }

  // Starts processing request by checking extended reporting and incognito
  // conditions.
  void Start();

  // Cancels the current request. |timed_out| indicates if this cancellation is
  // due to timeout. This function will call Finish() to destroy |this|.
  void Cancel(bool timed_out);

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

  const std::vector<std::string> matching_domains() const& {
    return matching_domains_;
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

  // WebContentsObserver implementation
  void WebContentsDestroyed() override;

 protected:
  friend class base::RefCountedThreadSafe<PasswordProtectionRequest>;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
  // Collects visual features from the current login page.
  void CollectVisualFeatures();

  // Processes the screenshot of the login page into visual features.
  void OnScreenshotTaken(const SkBitmap& bitmap);

  // Called when the visual feature extraction is complete.
  void OnVisualFeatureCollectionDone(
      std::unique_ptr<VisualFeatures> visual_features);

  // Called when the DOM feature extraction is complete.
  void OnGetDomFeatures(mojom::PhishingDetectorResult result,
                        const std::string& verdict);

  // Called when the DOM feature extraction times out.
  void OnGetDomFeatureTimeout();

  // If appropriate, collects visual features, otherwise continues on to sending
  // the request.
  void MaybeCollectVisualFeatures();
#endif

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

  // If this request is for unfamiliar login page or for a password reuse event.
  const LoginReputationClientRequest::TriggerType trigger_type_;

  // If there is a password field on the page.
  const bool password_field_exists_;

  // When request is sent.
  base::TimeTicks request_start_time_;

  // SimpleURLLoader instance for sending request and receiving response.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The PasswordProtectionService instance owns |this|.
  // Can only be accessed on UI thread.
  PasswordProtectionService* password_protection_service_;

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

#if BUILDFLAG(FULL_SAFE_BROWSING)
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
#endif

  base::WeakPtrFactory<PasswordProtectionRequest> weakptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PasswordProtectionRequest);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_H_
