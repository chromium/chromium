// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SETTINGS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SETTINGS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// A struct representing a custom message and associated "learn more" URL. These
// are scoped to a tag.
struct CustomMessageData {
  std::u16string message;
  GURL learn_more_url;
};

// A struct representing tag-specific settings that are applied to an analysis
// which includes that tag.
struct TagSettings {
  CustomMessageData custom_message;
  bool requires_justification = false;
};

// Enum representing if an analysis should block further interactions with the
// browser until its verdict is obtained.
enum class BlockUntilVerdict {
  kNoBlock = 0,
  kBlock = 1,
};

// Enum representing if an analysis should block further interactions with the
// browser if an error occurs.
enum class DefaultAction {
  kAllow = 0,
  kBlock = 1,
};

// Struct holding settings data specific to a cloud analysis.
struct CloudAnalysisSettings {
  CloudAnalysisSettings();
  CloudAnalysisSettings(CloudAnalysisSettings&&);
  CloudAnalysisSettings& operator=(CloudAnalysisSettings&&);
  CloudAnalysisSettings(const CloudAnalysisSettings&);
  CloudAnalysisSettings& operator=(const CloudAnalysisSettings&);
  ~CloudAnalysisSettings();

  // The URL of the server that performs an analysis in the cloud.
  GURL analysis_url;

  // The DM token to be used for scanning. May be empty, for example if this
  // scan is initiated by APP or for a local content analysis.
  std::string dm_token;

  // The scanning limit for all data passed to cloud content analysis.
  size_t max_file_size;
};

// Struct holding settings data specific to a local analysis.
struct LocalAnalysisSettings {
  LocalAnalysisSettings();
  LocalAnalysisSettings(LocalAnalysisSettings&&);
  LocalAnalysisSettings& operator=(LocalAnalysisSettings&&);
  LocalAnalysisSettings(const LocalAnalysisSettings&);
  LocalAnalysisSettings& operator=(const LocalAnalysisSettings&);
  ~LocalAnalysisSettings();

  std::string local_path;
  bool user_specific = false;
  base::raw_span<const char* const> subject_names;
  // The scanning limit for pasted text and image in local content analysis.
  size_t max_file_size;
  // Arrays of base64 encoded signing key signatures.
  std::vector<std::string> verification_signatures;
};

class CloudOrLocalAnalysisSettings
    : public absl::variant<CloudAnalysisSettings, LocalAnalysisSettings> {
 public:
  CloudOrLocalAnalysisSettings();
  explicit CloudOrLocalAnalysisSettings(CloudAnalysisSettings settings);
  explicit CloudOrLocalAnalysisSettings(LocalAnalysisSettings settings);
  CloudOrLocalAnalysisSettings(CloudOrLocalAnalysisSettings&&);
  CloudOrLocalAnalysisSettings& operator=(CloudOrLocalAnalysisSettings&&);
  CloudOrLocalAnalysisSettings(const CloudOrLocalAnalysisSettings&);
  CloudOrLocalAnalysisSettings& operator=(const CloudOrLocalAnalysisSettings&);

  ~CloudOrLocalAnalysisSettings();

  // Helpers for convenient check of the underlying variant.
  bool is_cloud_analysis() const;
  bool is_local_analysis() const;

  // Only call these when the CloudAnalysisSettings variant is used.
  const CloudAnalysisSettings& cloud_settings() const;
  const GURL& analysis_url() const;
  const std::string& dm_token() const;

  // Only call these when the LocalAnalysisSettings variant is used.
  const LocalAnalysisSettings& local_settings() const;
  const std::string local_path() const;
  bool user_specific() const;
  base::span<const char* const> subject_names() const;

  // Field accessible by both CloudAnalysisSettings and LocalAnalysisSettings.
  size_t max_file_size() const;
};

// Main struct holding settings data for the content analysis Connector.
struct AnalysisSettings {
  AnalysisSettings();
  AnalysisSettings(AnalysisSettings&&);
  AnalysisSettings& operator=(AnalysisSettings&&);
  ~AnalysisSettings();

  CloudOrLocalAnalysisSettings cloud_or_local_settings;
  std::map<std::string, TagSettings> tags;
  BlockUntilVerdict block_until_verdict = BlockUntilVerdict::kNoBlock;
  DefaultAction default_action = DefaultAction::kAllow;
  bool block_password_protected_files = false;
  bool block_large_files = false;

  // Minimum text size for BulkDataEntry scans. 0 means no minimum.
  size_t minimum_data_size = 100;

  // Indicates if the scan is made at the profile level, or at the browser level
  // if false.
  bool per_profile = false;

  // ClientMetadata to include in the scanning request(s). This is populated
  // based on OnSecurityEvent and the affiliation state of the browser.
  std::unique_ptr<ClientMetadata> client_metadata;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ANALYSIS_SETTINGS_H_
