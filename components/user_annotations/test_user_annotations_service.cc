// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/test_user_annotations_service.h"

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

TestUserAnnotationsService::TestUserAnnotationsService() = default;
TestUserAnnotationsService::~TestUserAnnotationsService() = default;

void TestUserAnnotationsService::ReplaceAllEntries(
    UserAnnotationsEntries entries) {
  entries_ = std::move(entries);
}

void TestUserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(UserAnnotationsEntries)> callback) {
  std::move(callback).Run(entries_);
}

}  // namespace user_annotations
