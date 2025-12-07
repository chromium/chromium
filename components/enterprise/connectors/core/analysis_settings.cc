// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/analysis_settings.h"

#include <variant>

namespace enterprise_connectors {

CloudAnalysisSettings::CloudAnalysisSettings() = default;
CloudAnalysisSettings::CloudAnalysisSettings(CloudAnalysisSettings&&) = default;
CloudAnalysisSettings& CloudAnalysisSettings::operator=(
    CloudAnalysisSettings&&) = default;
CloudAnalysisSettings::CloudAnalysisSettings(const CloudAnalysisSettings&) =
    default;
CloudAnalysisSettings& CloudAnalysisSettings::operator=(
    const CloudAnalysisSettings&) = default;
CloudAnalysisSettings::~CloudAnalysisSettings() = default;

LocalAnalysisSettings::LocalAnalysisSettings() = default;
LocalAnalysisSettings::LocalAnalysisSettings(LocalAnalysisSettings&&) = default;
LocalAnalysisSettings& LocalAnalysisSettings::operator=(
    LocalAnalysisSettings&&) = default;
LocalAnalysisSettings::LocalAnalysisSettings(const LocalAnalysisSettings&) =
    default;
LocalAnalysisSettings& LocalAnalysisSettings::operator=(
    const LocalAnalysisSettings&) = default;
LocalAnalysisSettings::~LocalAnalysisSettings() = default;

CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings() = default;
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    CloudAnalysisSettings settings)
    : std::variant<CloudAnalysisSettings, LocalAnalysisSettings>(
          std::move(settings)) {}
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    LocalAnalysisSettings settings)
    : std::variant<CloudAnalysisSettings, LocalAnalysisSettings>(
          std::move(settings)) {}
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    CloudOrLocalAnalysisSettings&&) = default;
CloudOrLocalAnalysisSettings& CloudOrLocalAnalysisSettings::operator=(
    CloudOrLocalAnalysisSettings&&) = default;
CloudOrLocalAnalysisSettings::CloudOrLocalAnalysisSettings(
    const CloudOrLocalAnalysisSettings&) = default;
CloudOrLocalAnalysisSettings& CloudOrLocalAnalysisSettings::operator=(
    const CloudOrLocalAnalysisSettings&) = default;
CloudOrLocalAnalysisSettings::~CloudOrLocalAnalysisSettings() = default;

bool CloudOrLocalAnalysisSettings::is_cloud_analysis() const {
  return std::holds_alternative<CloudAnalysisSettings>(*this);
}

bool CloudOrLocalAnalysisSettings::is_local_analysis() const {
  return std::holds_alternative<LocalAnalysisSettings>(*this);
}

const CloudAnalysisSettings& CloudOrLocalAnalysisSettings::cloud_settings()
    const {
  DCHECK(is_cloud_analysis());
  return std::get<CloudAnalysisSettings>(*this);
}

const GURL& CloudOrLocalAnalysisSettings::analysis_url() const {
  DCHECK(std::holds_alternative<CloudAnalysisSettings>(*this));
  return std::get<CloudAnalysisSettings>(*this).analysis_url;
}

const std::string& CloudOrLocalAnalysisSettings::dm_token() const {
  DCHECK(std::holds_alternative<CloudAnalysisSettings>(*this));
  return std::get<CloudAnalysisSettings>(*this).dm_token;
}

const LocalAnalysisSettings& CloudOrLocalAnalysisSettings::local_settings()
    const {
  DCHECK(is_local_analysis());
  return std::get<LocalAnalysisSettings>(*this);
}

const std::string CloudOrLocalAnalysisSettings::local_path() const {
  DCHECK(std::holds_alternative<LocalAnalysisSettings>(*this));
  return std::get<LocalAnalysisSettings>(*this).local_path;
}

bool CloudOrLocalAnalysisSettings::user_specific() const {
  DCHECK(std::holds_alternative<LocalAnalysisSettings>(*this));
  return std::get<LocalAnalysisSettings>(*this).user_specific;
}

base::span<const char* const> CloudOrLocalAnalysisSettings::subject_names()
    const {
  DCHECK(std::holds_alternative<LocalAnalysisSettings>(*this));
  return std::get<LocalAnalysisSettings>(*this).subject_names;
}

std::vector<std::string> CloudOrLocalAnalysisSettings::verification_signatures()
    const {
  DCHECK(std::holds_alternative<LocalAnalysisSettings>(*this));
  return std::get<LocalAnalysisSettings>(*this).verification_signatures;
}

size_t CloudOrLocalAnalysisSettings::max_file_size() const {
  if (is_local_analysis()) {
    return std::get<LocalAnalysisSettings>(*this).max_file_size;
  } else {
    return std::get<CloudAnalysisSettings>(*this).max_file_size;
  }
}

AnalysisSettings::AnalysisSettings() = default;
AnalysisSettings::AnalysisSettings(AnalysisSettings&&) = default;
AnalysisSettings& AnalysisSettings::operator=(AnalysisSettings&&) = default;
AnalysisSettings::~AnalysisSettings() = default;

}  // namespace enterprise_connectors
