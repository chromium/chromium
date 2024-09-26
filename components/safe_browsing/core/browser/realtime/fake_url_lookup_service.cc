// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"

#include "components/enterprise/common/proto/connectors.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing::testing {

FakeRealTimeUrlLookupService::FakeRealTimeUrlLookupService()
    : safe_browsing::RealTimeUrlLookupServiceBase(
          /*url_loader_factory=*/nullptr,
          /*cache_manager=*/nullptr,
          /*get_user_population_callback=*/base::BindRepeating([]() {
            return safe_browsing::ChromeUserPopulation();
          }),
          /*referrer_chain_provider=*/nullptr,
          /*pref_service=*/nullptr,
          /*webui_delegate=*/nullptr) {}

bool FakeRealTimeUrlLookupService::CanPerformFullURLLookup() const {
  return true;
}

bool FakeRealTimeUrlLookupService::CanIncludeSubframeUrlInReferrerChain()
    const {
  return false;
}

bool FakeRealTimeUrlLookupService::CanCheckSafeBrowsingDb() const {
  return true;
}

bool FakeRealTimeUrlLookupService::CanCheckSafeBrowsingHighConfidenceAllowlist()
    const {
  return true;
}

bool FakeRealTimeUrlLookupService::CanSendRTSampleRequest() const {
  return false;
}

std::string FakeRealTimeUrlLookupService::GetUserEmail() const {
  return "test@user.com";
}

std::string FakeRealTimeUrlLookupService::GetBrowserDMTokenString() const {
  return "browser_dm_token";
}

std::string FakeRealTimeUrlLookupService::GetProfileDMTokenString() const {
  return "profile_dm_token";
}

std::unique_ptr<enterprise_connectors::ClientMetadata>
FakeRealTimeUrlLookupService::GetClientMetadata() const {
  return nullptr;
}

std::string FakeRealTimeUrlLookupService::GetMetricSuffix() const {
  return ".Mock";
}

void FakeRealTimeUrlLookupService::SendSampledRequest(
    const GURL& url,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID session_id) {}

bool FakeRealTimeUrlLookupService::CanCheckUrl(const GURL& url) {
  return true;
}

GURL FakeRealTimeUrlLookupService::GetRealTimeLookupUrl() const {
  return GURL();
}
net::NetworkTrafficAnnotationTag
FakeRealTimeUrlLookupService::GetTrafficAnnotationTag() const {
  return TRAFFIC_ANNOTATION_FOR_TESTS;
}

bool FakeRealTimeUrlLookupService::CanPerformFullURLLookupWithToken() const {
  return false;
}

int FakeRealTimeUrlLookupService::GetReferrerUserGestureLimit() const {
  return 0;
}

bool FakeRealTimeUrlLookupService::CanSendPageLoadToken() const {
  return false;
}

void FakeRealTimeUrlLookupService::GetAccessToken(
    const GURL& url,
    safe_browsing::RTLookupResponseCallback response_callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    SessionID session_id) {}

std::optional<std::string> FakeRealTimeUrlLookupService::GetDMTokenString()
    const {
  return std::nullopt;
}

bool FakeRealTimeUrlLookupService::ShouldIncludeCredentials() const {
  return false;
}

std::optional<base::Time>
FakeRealTimeUrlLookupService::GetMinAllowedTimestampForReferrerChains() const {
  return std::nullopt;
}

}  // namespace safe_browsing::testing
