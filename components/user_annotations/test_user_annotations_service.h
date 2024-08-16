// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_

#include <vector>

#include "components/user_annotations/user_annotations_service.h"

namespace optimization_guide::proto {
class UserAnnotationsEntry;
}  // namespace optimization_guide::proto

namespace user_annotations {

class TestUserAnnotationsService : public UserAnnotationsService {
 public:
  TestUserAnnotationsService();
  TestUserAnnotationsService(const TestUserAnnotationsService&) = delete;
  TestUserAnnotationsService& operator=(const TestUserAnnotationsService&) =
      delete;
  ~TestUserAnnotationsService() override;

  // Replaces all entries in the service with `entries`.
  void ReplaceAllEntries(
      std::vector<optimization_guide::proto::UserAnnotationsEntry> entries);

  // UserAnnotationsService:
  void RetrieveAllEntries(
      base::OnceCallback<
          void(std::vector<optimization_guide::proto::UserAnnotationsEntry>)>
          callback) override;

 private:
  // An in-memory representation of the "database" of user annotation entries.
  std::vector<optimization_guide::proto::UserAnnotationsEntry> entries_;
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_
