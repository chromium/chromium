// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

FormAnnotationResponse::FormAnnotationResponse(
    const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
        to_be_upserted_entries,
    const std::string& model_execution_id)
    : to_be_upserted_entries(to_be_upserted_entries),
      model_execution_id(model_execution_id) {}

FormAnnotationResponse::~FormAnnotationResponse() = default;

}  // namespace user_annotations
