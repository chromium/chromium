// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_model_storage.h"

ReadingListModelStorage::ReadingListModelStorage(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

ReadingListModelStorage::~ReadingListModelStorage() {}
