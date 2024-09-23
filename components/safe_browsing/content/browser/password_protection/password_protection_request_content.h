// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_CONTENT_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_CONTENT_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "components/safe_browsing/core/browser/password_protection/password_protection_request.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper_passkeys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/skia/include/core/SkBitmap.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

class GURL;

namespace content {
class WebContents;
}

namespace safe_browsing {

class PasswordProtectionCommitDeferringCondition;
class PasswordProtectionServiceBase;
class RequestCanceler;

using password_manager::metrics_util::PasswordType;

class PasswordProtectionRequestContent final
    : public PasswordProtectionRequest {
 public:
  // Creates a request instance for testing which will stop short of issuing
  // real requests. See prevent_initiating_url_loader_for_testing_ in the base
  // class.
  static scoped_refptr<PasswordProtectionRequest> CreateForTesting(
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

  PasswordProtectionRequestContent(
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

  // CancelableRequest implementation
  void Cancel(bool timed_out) override;

  content::WebContents* web_contents() const { return web_contents_; }

  // Keeps track of deferred navigations.
  void AddDeferredNavigation(
      PasswordProtectionCommitDeferringCondition& condition) {
    deferred_navigations_.insert(&condition);
  }

  void RemoveDeferredNavigation(
      PasswordProtectionCommitDeferringCondition& condition) {
    deferred_navigations_.erase(&condition);
  }

  // Resumes any navigations that were deferred waiting on this request or any
  // associated modal warning dialog.
  void ResumeDeferredNavigations();

  std::set<
      raw_ptr<PasswordProtectionCommitDeferringCondition, SetExperimental>>&
  get_deferred_navigations_for_testing() {
    return deferred_navigations_;
  }

  base::WeakPtr<PasswordProtectionRequest> AsWeakPtr() override;

  base::WeakPtr<PasswordProtectionRequestContent> AsWeakPtrImpl() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class PasswordProtectionServiceTest;
  friend class ChromePasswordProtectionServiceTest;
  ~PasswordProtectionRequestContent() override;

  void MaybeLogPasswordReuseLookupEvent(
      RequestOutcome outcome,
      const LoginReputationClientResponse* response) override;

  void MaybeAddPingToWebUI(const std::string& oauth_token) override;

  void MaybeAddResponseToWebUI(
      const LoginReputationClientResponse& response) override;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  bool IsClientSideDetectionEnabled() override;

  // Extracts DOM features.
  void GetDomFeatures() override;

  // Called when the DOM feature extraction is complete.
  void OnGetDomFeatures(mojom::PhishingDetectorResult result,
                        std::optional<mojo_base::ProtoWrapper> verdict);

  void ExtractClientPhishingRequestFeatures(ClientPhishingRequest verdict);

  // Called when the DOM feature extraction times out.
  void OnGetDomFeatureTimeout();

  bool IsVisualFeaturesEnabled() override;

  // If appropriate, collects visual features, otherwise continues on to sending
  // the request.
  void MaybeCollectVisualFeatures() override;

  bool ShouldCollectVisualFeatures();

  // Collects visual features from the current login page.
  void CollectVisualFeatures();

  // Processes the screenshot of the login page into visual features.
  void OnScreenshotTaken(const SkBitmap& bitmap);

  // Called when the visual feature extraction is complete.
  void OnVisualFeatureCollectionDone(
      std::unique_ptr<VisualFeatures> visual_features);

#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_ANDROID)
  void SetReferringAppInfo() override;
#endif  // BUILDFLAG(IS_ANDROID)

  // WebContents of the password protection event.
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

  // Cancels the request when it is no longer valid.
  std::unique_ptr<RequestCanceler> request_canceler_;

  // Tracks navigations that are deferred on this request and any associated
  // modal dialog.
  std::set<raw_ptr<PasswordProtectionCommitDeferringCondition, SetExperimental>>
      deferred_navigations_;

  // If a request is sent, this is the token returned by the WebUI.
  int web_ui_token_;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // When we start extracting visual features.
  base::TimeTicks visual_feature_start_time_;

  // The Mojo pipe used for extracting DOM features from the renderer.
  mojo::AssociatedRemote<safe_browsing::mojom::PhishingDetector>
      phishing_detector_;

  // Whether the DOM features collection is finished, either by timeout or by
  // successfully gathering the features.
  bool dom_features_collection_complete_;
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

  base::WeakPtrFactory<PasswordProtectionRequestContent> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_PASSWORD_PROTECTION_PASSWORD_PROTECTION_REQUEST_CONTENT_H_
