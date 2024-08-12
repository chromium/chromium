// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {
class FormData;
}  // namespace autofill

namespace optimization_guide::proto {
class AXTreeUpdate;
class UserAnnotationsEntry;
}  // namespace optimization_guide::proto

namespace user_annotations {

struct Entry;

class UserAnnotationsService : public KeyedService {
 public:
  UserAnnotationsService();
  UserAnnotationsService(const UserAnnotationsService&) = delete;
  UserAnnotationsService& operator=(const UserAnnotationsService&) = delete;
  ~UserAnnotationsService() override;

  // Adds a form submission to the user annotations.
  void AddFormSubmission(
      const optimization_guide::proto::AXTreeUpdate& ax_tree_update,
      const autofill::FormData& form_data);

  // Retrieves all entries from the database. Invokes `callback` when complete.
  // Virtual for testing.
  virtual void RetrieveAllEntries(
      base::OnceCallback<
          void(std::vector<optimization_guide::proto::UserAnnotationsEntry>)>
          callback);

  // KeyedService:
  void Shutdown() override;

 private:
  // An in-memory representation of the "database" of user annotation entries.
  std::vector<Entry> entries_;

  int64_t entry_id_counter_ = 0;
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_
