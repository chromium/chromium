// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/empty_comments_service.h"

#include "base/functional/callback.h"
#include "base/uuid.h"

namespace collaboration::comments {

EmptyCommentsService::EmptyCommentsService() = default;

EmptyCommentsService::~EmptyCommentsService() = default;

bool EmptyCommentsService::IsInitialized() const {
  return true;
}

bool EmptyCommentsService::IsEmptyService() const {
  return true;
}

CommentId EmptyCommentsService::AddComment(
    const CollaborationId& collaboration_id,
    const GURL& url,
    const std::string& content,
    const std::optional<CommentId>& parent_comment_id,
    base::OnceCallback<void(bool)> success_callback) {
  return base::Uuid();
}

void EmptyCommentsService::EditComment(
    const CommentId& comment_id,
    const std::string& new_content,
    base::OnceCallback<void(bool)> success_callback) {}

void EmptyCommentsService::DeleteComment(
    const CommentId& comment_id,
    base::OnceCallback<void(bool)> success_callback) {}

void EmptyCommentsService::QueryComments(
    const FilterCriteria& filter_criteria,
    const PaginationCriteria& pagination_criteria,
    base::OnceCallback<void(QueryResult)> callback) {}

void EmptyCommentsService::AddObserver(CommentsObserver* observer,
                                       const FilterCriteria& filter_criteria) {}

void EmptyCommentsService::RemoveObserver(CommentsObserver* observer) {}

}  // namespace collaboration::comments
