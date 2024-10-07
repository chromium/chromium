// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_

#include <set>
#include <vector>

#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"

namespace autofill {
class FormStructure;
}

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
  void ReplaceAllEntries(UserAnnotationsEntries entries);

  // `AddFormSubmission()` will only import form data if
  // `should_import_form_data` is set to `true`.
  void SetShouldImportFormData(bool should_import_form_data) {
    should_import_form_data_ = should_import_form_data;
  }

  // Adds `host` to set of hosts that forms annotations are allowed on.
  void AddHostToFormAnnotationsAllowlist(const std::string& host);

  // UserAnnotationsService:
  bool ShouldAddFormSubmissionForURL(const GURL& url) override;
  void AddFormSubmission(const GURL& url,
                         const std::string& title,
                         optimization_guide::proto::AXTreeUpdate ax_tree_update,
                         std::unique_ptr<autofill::FormStructure> form,
                         ImportFormCallback callback) override;
  void RetrieveAllEntries(
      base::OnceCallback<void(UserAnnotationsEntries)> callback) override;
  void RemoveEntry(EntryID entry_id, base::OnceClosure callback) override;
  void RemoveAllEntries(base::OnceClosure callback) override;
  void RemoveAnnotationsInRange(const base::Time& delete_begin,
                                const base::Time& delete_end) override;

  // Returns the number of entries set via `ReplaceAllEntries()` ignoring
  // the `begin` and `end` arguments.
  void GetCountOfValuesContainedBetween(
      base::Time begin,
      base::Time end,
      base::OnceCallback<void(int)> callback) override;
  size_t count_entries_retrieved() const { return count_entries_retrieved_; }

  std::pair<base::Time, base::Time> last_received_remove_annotations_in_range()
      const {
    return last_received_remove_annotations_in_range_;
  }

 private:
  // An in-memory representation of the "database" of user annotation entries.
  std::vector<optimization_guide::proto::UserAnnotationsEntry> entries_;

  // Used in `AddFormSubmission()` to decide if form data should be imported.
  bool should_import_form_data_ = true;

  // Hosts allowed for forms annotations.
  std::set<std::string> allowed_forms_annotations_hosts_;

  // Saves the last call for `RemoveAnnotationsInRange`.
  std::pair<base::Time, base::Time> last_received_remove_annotations_in_range_;

  // The number of times entries have been retrieved.
  size_t count_entries_retrieved_ = 0;
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_TEST_USER_ANNOTATIONS_SERVICE_H_
