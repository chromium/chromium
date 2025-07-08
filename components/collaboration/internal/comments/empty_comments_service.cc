// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/empty_comments_service.h"

namespace collaboration::comments {

EmptyCommentsService::EmptyCommentsService() = default;

EmptyCommentsService::~EmptyCommentsService() = default;

bool EmptyCommentsService::IsInitialized() const {
  return true;
}

bool EmptyCommentsService::IsEmptyService() const {
  return true;
}

}  // namespace collaboration::comments
