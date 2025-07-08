// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/comments_service_impl.h"

namespace collaboration::comments {

CommentsServiceImpl::CommentsServiceImpl() = default;

CommentsServiceImpl::~CommentsServiceImpl() = default;

bool CommentsServiceImpl::IsInitialized() const {
  return false;
}

bool CommentsServiceImpl::IsEmptyService() const {
  return false;
}

}  // namespace collaboration::comments
