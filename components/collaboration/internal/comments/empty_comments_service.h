// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_EMPTY_COMMENTS_SERVICE_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_EMPTY_COMMENTS_SERVICE_H_

#include "components/collaboration/public/comments/comments_service.h"

namespace collaboration::comments {

// An empty implementation of CommentsService that does nothing.
class EmptyCommentsService : public CommentsService {
 public:
  EmptyCommentsService();
  ~EmptyCommentsService() override;

  // Disallow copy/assign.
  EmptyCommentsService(const EmptyCommentsService&) = delete;
  EmptyCommentsService& operator=(const EmptyCommentsService&) = delete;

  // CommentsService implementation.
  bool IsInitialized() const override;
  bool IsEmptyService() const override;
};

}  // namespace collaboration::comments

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_EMPTY_COMMENTS_SERVICE_H_
