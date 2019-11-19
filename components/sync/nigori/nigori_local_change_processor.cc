// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/nigori_local_change_processor.h"

#include <memory>

namespace syncer {

NigoriMetadataBatch::NigoriMetadataBatch() = default;

NigoriMetadataBatch::NigoriMetadataBatch(NigoriMetadataBatch&& other)
    : model_type_state(std::move(other.model_type_state)),
      entity_metadata(std::move(other.entity_metadata)) {}

NigoriMetadataBatch::~NigoriMetadataBatch() = default;

}  // namespace syncer
