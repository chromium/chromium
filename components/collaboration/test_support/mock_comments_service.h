// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COMMENTS_SERVICE_H_
#define COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COMMENTS_SERVICE_H_

#include "components/collaboration/public/comments/comments_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace collaboration::comments {

class MockCommentsService : public CommentsService {
 public:
  MockCommentsService();
  ~MockCommentsService() override;

  // CommentsService implementation.
  MOCK_METHOD(bool, IsInitialized, (), (const, override));
  MOCK_METHOD(bool, IsEmptyService, (), (const, override));
  MOCK_METHOD(CommentId,
              AddComment,
              (const CollaborationId&,
               const GURL&,
               const std::string&,
               const std::optional<CommentId>&,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              EditComment,
              (const CommentId&,
               const std::string&,
               base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              DeleteComment,
              (const CommentId&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              QueryComments,
              (const FilterCriteria&,
               const PaginationCriteria&,
               base::OnceCallback<void(QueryResult)>),
              (override));
  MOCK_METHOD(void,
              AddObserver,
              (CommentsObserver*, const FilterCriteria&),
              (override));
  MOCK_METHOD(void, RemoveObserver, (CommentsObserver*), (override));
};

}  // namespace collaboration::comments

#endif  // COMPONENTS_COLLABORATION_TEST_SUPPORT_MOCK_COMMENTS_SERVICE_H_
