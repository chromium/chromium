// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace private_insights {

class COMPONENT_EXPORT(PRIVATE_INSIGHTS) PrivateInsightsService
    : public KeyedService {
 public:
  PrivateInsightsService();
  ~PrivateInsightsService() override;

  PrivateInsightsService(const PrivateInsightsService&) = delete;
  PrivateInsightsService& operator=(const PrivateInsightsService&) = delete;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
