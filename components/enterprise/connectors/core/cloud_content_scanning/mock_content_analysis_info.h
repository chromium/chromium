// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MOCK_CONTENT_ANALYSIS_INFO_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MOCK_CONTENT_ANALYSIS_INFO_H_

#include <string>

#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace enterprise_connectors {

// Mock implementation of the ContentAnalysisInfoBase.
class MockContentAnalysisInfoBase : public ContentAnalysisInfoBase {
 public:
  MockContentAnalysisInfoBase();
  ~MockContentAnalysisInfoBase() override;

  MOCK_METHOD(void,
              InitializeRequest,
              (BinaryUploadRequest * request,
               bool include_enterprise_only_fields),
              (override));
  MOCK_METHOD(const AnalysisSettings&, settings, (), (const, override));
  MOCK_METHOD(signin::IdentityManager*,
              identity_manager,
              (),
              (const, override));
  MOCK_METHOD(int, user_action_requests_count, (), (const, override));
  MOCK_METHOD(std::string, tab_title, (), (const, override));
  MOCK_METHOD(std::string, user_action_id, (), (const, override));
  MOCK_METHOD(std::string, email, (), (const, override));
  MOCK_METHOD(const GURL&, url, (), (const, override));
  MOCK_METHOD(const GURL&, tab_url, (), (const, override));
  MOCK_METHOD(ContentAnalysisRequest::Reason, reason, (), (const, override));
  MOCK_METHOD(
      (google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>),
      referrer_chain,
      (),
      (const, override));
  MOCK_METHOD(google::protobuf::RepeatedPtrField<std::string>,
              frame_url_chain,
              (),
              (const, override));
  MOCK_METHOD(std::string, GetContentAreaAccountEmail, (), (const, override));
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_MOCK_CONTENT_ANALYSIS_INFO_H_
