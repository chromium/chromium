// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/cast_dialog_model.h"

namespace media_router {

CastDialogModel::CastDialogModel() = default;

CastDialogModel::CastDialogModel(const CastDialogModel& other) = default;

CastDialogModel::~CastDialogModel() = default;

std::optional<size_t> CastDialogModel::GetFirstActiveSinkIndex() const {
  for (size_t i = 0; i < media_sinks_.size(); i++) {
    if (!media_sinks_.at(i).route)
      return i;
  }
  return std::nullopt;
}

}  // namespace media_router
