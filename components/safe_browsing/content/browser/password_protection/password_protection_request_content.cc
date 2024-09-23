// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/password_protection/password_protection_request_content.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_commit_deferring_condition.h"
#include "components/safe_browsing/content/browser/password_protection/password_protection_service.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/password_protection/request_canceler.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/common/visual_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_ANDROID)
#include "ui/android/view_android.h"
#endif

namespace safe_browsing {

namespace {

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
// The maximum time to wait for DOM features to be collected, in milliseconds.
const int kDomFeatureTimeoutMs = 3000;

void ExtractVisualFeaturesAndReplyOnUIThread(
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::unique_ptr<VisualFeatures>)>
        ui_thread_callback) {
  std::unique_ptr<VisualFeatures> visual_features =
      visual_utils::ExtractVisualFeatures(bitmap);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(ui_thread_callback),
                                std::move(visual_features)));
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

void LogCSDCacheContainsImages(bool contains_images) {
  base::UmaHistogramBoolean("PasswordProtection.CSDCacheContainsImages",
                            contains_images);
}

void LogCSDCacheContainsDebuggingMetadata(
    LoginReputationClientRequest::DebuggingMetadata* debugging_metadata) {
  base::UmaHistogramBoolean(
      "PasswordProtection.CSDCacheContainsDebuggingMetadata",
      debugging_metadata != nullptr);
}

}  // namespace

// static
scoped_refptr<PasswordProtectionRequest>
PasswordProtectionRequestContent::CreateForTesting(
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
    int request_timeout_in_ms) {
  scoped_refptr<PasswordProtectionRequest> request(
      new PasswordProtectionRequestContent(
          web_contents, main_frame_url, password_form_action,
          password_form_frame_url, mime_type, username, password_type,
          matching_reused_credentials, type, password_field_exists, pps,
          request_timeout_in_ms));
  static_cast<PasswordProtectionRequestContent*>(request.get())
      ->prevent_initiating_url_loader_for_testing_ = true;
  return request;
}

PasswordProtectionRequestContent::PasswordProtectionRequestContent(
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
    int request_timeout_in_ms)
    : PasswordProtectionRequest(content::GetUIThreadTaskRunner({}),
                                content::GetIOThreadTaskRunner({}),
                                main_frame_url,
                                password_form_action,
                                password_form_frame_url,
                                mime_type,
                                username,
                                password_type,
                                matching_reused_credentials,
                                type,
                                password_field_exists,
                                pps,
                                request_timeout_in_ms),
      web_contents_(web_contents) {
  request_canceler_ = RequestCanceler::CreateRequestCanceler(
      weak_factory_.GetWeakPtr(), web_contents);
}

PasswordProtectionRequestContent::~PasswordProtectionRequestContent() = default;

void PasswordProtectionRequestContent::Cancel(bool timed_out) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If request is canceled because |password_protection_service_| is shutting
  // down, ignore all these deferred navigations.
  if (!timed_out) {
    deferred_navigations_.clear();
  }
  PasswordProtectionRequest::Cancel(timed_out);
}

void PasswordProtectionRequestContent::ResumeDeferredNavigations() {
  for (auto itr = deferred_navigations_.begin();
       itr != deferred_navigations_.end();) {
    // ResumeNavigation will lead to the condition being destroyed which may
    // remove it from deferred_navigations_. Make sure we move the iterator
    // before calling it.
    PasswordProtectionCommitDeferringCondition* condition = *itr;
    itr++;
    condition->ResumeNavigation();
  }

  deferred_navigations_.clear();
}

base::WeakPtr<PasswordProtectionRequest>
PasswordProtectionRequestContent::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PasswordProtectionRequestContent::MaybeLogPasswordReuseLookupEvent(
    RequestOutcome outcome,
    const LoginReputationClientResponse* response) {
  PasswordProtectionService* service =
      static_cast<PasswordProtectionService*>(password_protection_service());
  service->MaybeLogPasswordReuseLookupEvent(web_contents_, outcome,
                                            password_type(), response);
}

void PasswordProtectionRequestContent::MaybeAddPingToWebUI(
    const std::string& oauth_token) {
  web_ui_token_ = WebUIInfoSingleton::GetInstance()->AddToPGPings(
      *request_proto_, oauth_token);
}

void PasswordProtectionRequestContent::MaybeAddResponseToWebUI(
    const LoginReputationClientResponse& response) {
  WebUIInfoSingleton::GetInstance()->AddToPGResponses(web_ui_token_, response);
}

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
bool PasswordProtectionRequestContent::IsClientSideDetectionEnabled() {
  return true;
}

bool PasswordProtectionRequestContent::IsVisualFeaturesEnabled() {
  return true;
}

void PasswordProtectionRequestContent::GetDomFeatures() {
  ClientSideDetectionFeatureCache* feature_cache_map =
      ClientSideDetectionFeatureCache::FromWebContents(web_contents_);
  if (feature_cache_map) {
    if (password_protection_service()->IsExtendedReporting() &&
        base::FeatureList::IsEnabled(
            kClientSideDetectionDebuggingMetadataCache) &&
        trigger_type() == LoginReputationClientRequest::PASSWORD_REUSE_EVENT) {
      LoginReputationClientRequest::DebuggingMetadata* debugging_metadata =
          feature_cache_map->GetDebuggingMetadataForURL(main_frame_url());

      LogCSDCacheContainsDebuggingMetadata(debugging_metadata);
      if (debugging_metadata) {
        request_proto_->mutable_csd_debugging_metadata()->Swap(
            debugging_metadata);
        feature_cache_map->RemoveDebuggingMetadataForURL(main_frame_url());

        // We expect the debugging metadata size to be non-zero in most cases
        // because we'd expect that at least the Preclassification checks
        // would have ran if the tab has loaded the page.

        base::UmaHistogramCounts100(
            "PasswordProtection.CSDCacheDebuggingMetadataSizeAtHit",
            feature_cache_map->GetTotalDebuggingMetadataMapEntriesSize());
      }
    }

    ClientPhishingRequest* verdict =
        feature_cache_map->GetVerdictForURL(main_frame_url());
    if (verdict) {
      dom_features_collection_complete_ = true;

      LogCSDCacheContainsImages(
          verdict->mutable_visual_features()->has_image());

      base::UmaHistogramCounts100(
          "PasswordProtection.CSDCacheSizeAtHit",
          feature_cache_map->GetTotalVerdictEntriesSize());

      ExtractClientPhishingRequestFeatures(*verdict);

      if (!request_proto_->mutable_visual_features()->has_image() &&
          IsVisualFeaturesEnabled()) {
        MaybeCollectVisualFeatures();
      } else {
        SendRequest();
      }
      return;
    } else {
      LogCSDCacheContainsImages(false);
    }
  }

  phishing_detector_.reset();

  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&phishing_detector_);

  dom_features_collection_complete_ = false;
  phishing_detector_->StartPhishingDetection(
      main_frame_url(),
      base::BindRepeating(&PasswordProtectionRequestContent::OnGetDomFeatures,
                          weak_factory_.GetWeakPtr()));

  content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::UI)
      ->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &PasswordProtectionRequestContent::OnGetDomFeatureTimeout,
              weak_factory_.GetWeakPtr()),
          base::Milliseconds(kDomFeatureTimeoutMs));
}

void PasswordProtectionRequestContent::OnGetDomFeatures(
    mojom::PhishingDetectorResult result,
    std::optional<mojo_base::ProtoWrapper> verdict) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (dom_features_collection_complete_)
    return;

  if (result != mojom::PhishingDetectorResult::SUCCESS &&
      result != mojom::PhishingDetectorResult::INVALID_SCORE)
    return;

  base::UmaHistogramBoolean(
      "PasswordProtection.SuccessfulPhishingDetectionWithinTimeout", true);

  dom_features_collection_complete_ = true;
  if (verdict.has_value()) {
    auto dom_features_request = verdict->As<ClientPhishingRequest>();
    if (dom_features_request.has_value()) {
      ExtractClientPhishingRequestFeatures(
          std::move(dom_features_request.value()));
    }
  }

  if (!request_proto_->mutable_visual_features()->has_image() &&
      IsVisualFeaturesEnabled()) {
    MaybeCollectVisualFeatures();
  } else {
    SendRequest();
  }
}

void PasswordProtectionRequestContent::ExtractClientPhishingRequestFeatures(
    ClientPhishingRequest verdict) {
  for (const ClientPhishingRequest::Feature& feature : verdict.feature_map()) {
    DomFeatures::Feature* new_feature =
        request_proto_->mutable_dom_features()->add_feature_map();
    new_feature->set_name(feature.name());
    new_feature->set_value(feature.value());
  }

  for (const ClientPhishingRequest::Feature& feature :
       verdict.non_model_feature_map()) {
    DomFeatures::Feature* new_feature =
        request_proto_->mutable_dom_features()->add_feature_map();
    new_feature->set_name(feature.name());
    new_feature->set_value(feature.value());
  }

  request_proto_->mutable_dom_features()->mutable_shingle_hashes()->Swap(
      verdict.mutable_shingle_hashes());
  request_proto_->mutable_dom_features()->set_model_version(
      verdict.model_version());

  if (ShouldCollectVisualFeatures() && verdict.mutable_visual_features()) {
    request_proto_->mutable_visual_features()->Swap(
        verdict.mutable_visual_features());
  }
}

void PasswordProtectionRequestContent::OnGetDomFeatureTimeout() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!dom_features_collection_complete_) {
    dom_features_collection_complete_ = true;
    base::UmaHistogramBoolean(
        "PasswordProtection.SuccessfulPhishingDetectionWithinTimeout", false);
    if (!request_proto_->mutable_visual_features()->has_image() &&
        IsVisualFeaturesEnabled()) {
      MaybeCollectVisualFeatures();
    } else {
      SendRequest();
    }
  }
}

bool PasswordProtectionRequestContent::ShouldCollectVisualFeatures() {
  // TODO(crbug.com/40926113): Unify this with the code to populate
  // content_area_width and content_area_height on desktop.
#if BUILDFLAG(IS_ANDROID)
  if (password_protection_service()->IsExtendedReporting() &&
      !password_protection_service()->IsIncognito()) {
    content::RenderWidgetHostView* view =
        web_contents_ ? web_contents_->GetRenderWidgetHostView() : nullptr;
    if (view && view->GetNativeView()) {
      gfx::SizeF content_area_size = view->GetNativeView()->viewport_size();
      request_proto_->set_content_area_height(content_area_size.height());
      request_proto_->set_content_area_width(content_area_size.width());
    }
  }
#endif

  visual_utils::CanExtractVisualFeaturesResult
      can_extract_visual_features_result =
#if BUILDFLAG(IS_ANDROID)
          visual_utils::CanExtractVisualFeatures(
              password_protection_service()->IsExtendedReporting(),
              password_protection_service()->IsIncognito(),
              gfx::Size(request_proto_->content_area_width(),
                        request_proto_->content_area_height()));
#else
          visual_utils::CanExtractVisualFeatures(
              password_protection_service()->IsExtendedReporting(),
              password_protection_service()->IsIncognito(),
              gfx::Size(request_proto_->content_area_width(),
                        request_proto_->content_area_height()),
              zoom::ZoomController::GetZoomLevelForWebContents(web_contents_));
#endif

  base::UmaHistogramEnumeration("PasswordProtection.VisualFeaturesClearReason",
                                can_extract_visual_features_result);

  // Once the DOM features are collected, either collect visual features, or go
  // straight to sending the ping.
  bool trigger_type_supports_visual_features =
      trigger_type() == LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE ||
      trigger_type() == LoginReputationClientRequest::PASSWORD_REUSE_EVENT;

  return trigger_type_supports_visual_features &&
         can_extract_visual_features_result ==
             visual_utils::CanExtractVisualFeaturesResult::
                 kCanExtractVisualFeatures;
}

void PasswordProtectionRequestContent::MaybeCollectVisualFeatures() {
  if (ShouldCollectVisualFeatures()) {
    CollectVisualFeatures();
  } else {
    SendRequest();
  }
}

void PasswordProtectionRequestContent::CollectVisualFeatures() {
  content::RenderWidgetHostView* view =
      web_contents_ ? web_contents_->GetRenderWidgetHostView() : nullptr;

  if (!view) {
    SendRequest();
    return;
  }

  visual_feature_start_time_ = base::TimeTicks::Now();

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(&PasswordProtectionRequestContent::OnScreenshotTaken,
                     weak_factory_.GetWeakPtr()));
}

void PasswordProtectionRequestContent::OnScreenshotTaken(
    const SkBitmap& screenshot) {
  // Do the feature extraction on a worker thread, to avoid blocking the UI.
  auto ui_thread_callback = base::BindOnce(
      &PasswordProtectionRequestContent::OnVisualFeatureCollectionDone,
      weak_factory_.GetWeakPtr());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExtractVisualFeaturesAndReplyOnUIThread, screenshot,
                     std::move(ui_thread_callback)));
}

void PasswordProtectionRequestContent::OnVisualFeatureCollectionDone(
    std::unique_ptr<VisualFeatures> visual_features) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  request_proto_->mutable_visual_features()->Swap(visual_features.get());

  base::UmaHistogramMediumTimes(
      "PasswordProtection.VisualFeatureExtractionDuration",
      base::TimeTicks::Now() - visual_feature_start_time_);

  SendRequest();
}
#endif  // BUILDFLAG(SAFE_BROWSING_AVAILABLE)

#if BUILDFLAG(IS_ANDROID)
void PasswordProtectionRequestContent::SetReferringAppInfo() {
  PasswordProtectionService* service =
      static_cast<PasswordProtectionService*>(password_protection_service());
  LoginReputationClientRequest::ReferringAppInfo referring_app_info =
      service->GetReferringAppInfo(web_contents_);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordProtection.RequestReferringAppSource",
      referring_app_info.referring_app_source(),
      LoginReputationClientRequest::ReferringAppInfo::ReferringAppSource_MAX +
          1);
  *request_proto_->mutable_referring_app_info() = std::move(referring_app_info);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace safe_browsing
