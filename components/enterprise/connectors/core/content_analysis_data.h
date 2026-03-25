// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_DATA_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_DATA_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Input data for content analysis for either pasted data, files or printed
// data. Used as an input for functions like
// ContentAnalysisDelegate::CreateForWebContents().
struct ContentAnalysisData {
  ContentAnalysisData();
  ContentAnalysisData(const ContentAnalysisData&) = delete;
  ContentAnalysisData& operator=(const ContentAnalysisData&) = delete;
  ContentAnalysisData(ContentAnalysisData&& other);
  ContentAnalysisData& operator=(ContentAnalysisData&& other);
  ~ContentAnalysisData();

  // URL of the page that is to receive sensitive data.
  GURL url;

  // UTF-8 encoded text data to scan, such as plain text, URLs, HTML, etc.
  std::vector<std::string> text;

  // Binary image data to scan, such as png, svg, etc (here we assume the data
  // struct holds one image only).
  std::string image;

  // List of files to scan.
  std::vector<base::FilePath> paths;

  // Page to be printed to scan.
  base::ReadOnlySharedMemoryRegion page;

  // Printer name of the page being sent to, empty for non-print actions.
  std::string printer_name;

  // TODO(b/283108167): Delete or send printer type information to local
  // service partner.
  //  Printer type of the page being sent to, the default value is UNKNOWN.
  ContentMetaData::PrintMetadata::PrinterType printer_type =
      ContentMetaData::PrintMetadata::UNKNOWN;

  // The reason the scanning should happen. This should be populated at the
  // same time as fields like `text`, `paths`, `page`, etc. so that caller
  // code can let enterprise code know the user action triggering content
  // analysis.
  ContentAnalysisRequest::Reason reason = ContentAnalysisRequest::UNKNOWN;

  // The clipboard source of data being pasted into the browser. Empty for
  // non-clipboard pastes, and clipboard pastes in special cases (ex. OTR).
  ContentMetaData::CopiedTextSource clipboard_source;

  // The email for the content area user of the source of clipboard data.
  // Only populated for Workspace sites.
  std::string source_content_area_email;

  // The settings to use for the analysis of the data in this struct.
  AnalysisSettings settings;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONTENT_ANALYSIS_DATA_H_
