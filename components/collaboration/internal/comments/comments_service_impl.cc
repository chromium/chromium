// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/comments/comments_service_impl.h"

#include "base/functional/callback.h"
#include "base/notimplemented.h"
#include "base/uuid.h"

namespace collaboration::comments {

CommentsServiceImpl::CommentsServiceImpl() = default;

CommentsServiceImpl::~CommentsServiceImpl() = default;

bool CommentsServiceImpl::IsInitialized() const {
  return false;
}

bool CommentsServiceImpl::IsEmptyService() const {
  return false;
}

CommentId CommentsServiceImpl::AddComment(
    const CollaborationId& collaboration_id,
    const GURL& url,
    const std::string& content,
    const std::optional<CommentId>& parent_comment_id,
    base::OnceCallback<void(bool)> success_callback) {
  NOTIMPLEMENTED();
  return base::Uuid::GenerateRandomV4();
}

void CommentsServiceImpl::EditComment(
    const CommentId& comment_id,
    const std::string& new_content,
    base::OnceCallback<void(bool)> success_callback) {
  NOTIMPLEMENTED();
}

void CommentsServiceImpl::DeleteComment(
    const CommentId& comment_id,
    base::OnceCallback<void(bool)> success_callback) {
  NOTIMPLEMENTED();
}

void CommentsServiceImpl::QueryComments(
    const FilterCriteria& filter_criteria,
    const PaginationCriteria& pagination_criteria,
    base::OnceCallback<void(QueryResult)> callback) {
  NOTIMPLEMENTED();
}

void CommentsServiceImpl::AddObserver(CommentsObserver* observer,
                                      const FilterCriteria& filter_criteria) {
  NOTIMPLEMENTED();
}

void CommentsServiceImpl::RemoveObserver(CommentsObserver* observer) {
  NOTIMPLEMENTED();
}

}  // namespace collaboration::comments
