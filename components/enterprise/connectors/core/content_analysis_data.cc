// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/content_analysis_data.h"

namespace enterprise_connectors {

ContentAnalysisData::ContentAnalysisData() = default;
ContentAnalysisData::ContentAnalysisData(ContentAnalysisData&& other) = default;
ContentAnalysisData& ContentAnalysisData::operator=(
    ContentAnalysisData&& other) = default;
ContentAnalysisData::~ContentAnalysisData() = default;

}  // namespace enterprise_connectors
