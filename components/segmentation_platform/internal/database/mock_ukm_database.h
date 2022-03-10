// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_

#include "components/segmentation_platform/internal/database/ukm_database.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace segmentation_platform {

class MockUkmDatabase : public UkmDatabase {
 public:
  MockUkmDatabase();
  ~MockUkmDatabase() override;

  MOCK_METHOD1(UkmEntryAdded, void(ukm::mojom::UkmEntryPtr ukm_entry));

  MOCK_METHOD3(UkmSourceUrlUpdated,
               void(ukm::SourceId source_id,
                    const GURL& url,
                    bool is_validated));

  MOCK_METHOD1(OnUrlValidated, void(const GURL& url));

  MOCK_METHOD1(RemoveUrls, void(const std::vector<GURL>& urls));
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_MOCK_UKM_DATABASE_H_
