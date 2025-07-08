// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_COMMENTS_SERVICE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_COMMENTS_SERVICE_IMPL_H_

#include "components/collaboration/public/comments/comments_service.h"

namespace collaboration::comments {

// The implementation of the CommentsService.
class CommentsServiceImpl : public CommentsService {
 public:
  CommentsServiceImpl();
  ~CommentsServiceImpl() override;

  // Disallow copy/assign.
  CommentsServiceImpl(const CommentsServiceImpl&) = delete;
  CommentsServiceImpl& operator=(const CommentsServiceImpl&) = delete;

  // CommentsService implementation.
  bool IsInitialized() const override;
  bool IsEmptyService() const override;
};

}  // namespace collaboration::comments

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_COMMENTS_SERVICE_IMPL_H_
