// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_INFO_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_INFO_BASE_H_

#include <string>

#include "components/enterprise/common/proto/connectors.pb.h"

class GURL;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_connectors {

struct AnalysisSettings;

// Platform-agnostic interface providing data about a given content analysis
// action. This should be used as an abstraction layer to access information
// about some content analysis context when the exact action that triggered is
// not important (ex. when populating protos).
class ContentAnalysisInfoBase {
 public:
  // The `AnalysisSettings` that should be applied to the content analysis scan.
  virtual const AnalysisSettings& settings() const = 0;

  // The `signin::IdentityManager` that corresponds to the browser context where
  // content analysis is taking place.
  virtual signin::IdentityManager* identity_manager() const = 0;

  // These methods correspond to fields in `BinaryUploadService::Request`.
  virtual int user_action_requests_count() const = 0;
  virtual std::string tab_title() const = 0;
  virtual std::string user_action_id() const = 0;
  virtual std::string email() const = 0;
  virtual const GURL& url() const = 0;
  virtual const GURL& tab_url() const = 0;
  virtual ContentAnalysisRequest::Reason reason() const = 0;
  virtual google::protobuf::RepeatedPtrField<
      ::safe_browsing::ReferrerChainEntry>
  referrer_chain() const = 0;
  virtual google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const = 0;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_INFO_BASE_H_
