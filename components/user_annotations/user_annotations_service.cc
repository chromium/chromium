// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

UserAnnotationsService::UserAnnotationsService() = default;
UserAnnotationsService::~UserAnnotationsService() = default;

void UserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(std::vector<Entry>)> callback) {
  std::move(callback).Run(entries_);
}

void UserAnnotationsService::Shutdown() {}

}  // namespace user_annotations
