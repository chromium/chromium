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
  CommentId AddComment(
      const CollaborationId& collaboration_id,
      const GURL& url,
      const std::string& content,
      const std::optional<CommentId>& parent_comment_id,
      base::OnceCallback<void(bool)> success_callback) override;
  void EditComment(const CommentId& comment_id,
                   const std::string& new_content,
                   base::OnceCallback<void(bool)> success_callback) override;
  void DeleteComment(const CommentId& comment_id,
                     base::OnceCallback<void(bool)> success_callback) override;
  void QueryComments(const FilterCriteria& filter_criteria,
                     const PaginationCriteria& pagination_criteria,
                     base::OnceCallback<void(QueryResult)> callback) override;
  void AddObserver(CommentsObserver* observer,
                   const FilterCriteria& filter_criteria) override;
  void RemoveObserver(CommentsObserver* observer) override;
};

}  // namespace collaboration::comments

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COMMENTS_COMMENTS_SERVICE_IMPL_H_
