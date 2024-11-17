// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/form_parsing/password_field_prediction.h"

namespace password_manager {

struct FormPredictions;

struct FieldInfo {
  // Id of the PasswordManagerDriver which corresponds to the frame of the
  // field. Paired with the |field_id|, this identifies a field globally.
  int driver_id = -1;

  // The renderer id of a field.
  autofill::FieldRendererId field_id;

  // Signon realm of the form.
  std::string signon_realm;

  // Lowercased field value.
  std::u16string value;

  // Whether the field is likely to be an OTP field, based on its HTML
  // attributes.
  bool is_likely_otp;

  // The type of the field predicted by the server.
  autofill::FieldType type = autofill::FieldType::UNKNOWN_TYPE;

  // Predictions for the form containing the field.
  std::optional<FormPredictions> stored_predictions;

  FieldInfo(int driver_id,
            autofill::FieldRendererId field_id,
            std::string signon_realm,
            std::u16string value,
            bool is_likely_otp);
  FieldInfo(const FieldInfo&);
  FieldInfo& operator=(const FieldInfo&);
  ~FieldInfo();

  friend bool operator==(const FieldInfo& lhs, const FieldInfo& rhs) = default;
};

// Manages information about the last user-interacted fields, keeps
// the data and erases it once it becomes stale.
class FieldInfoManager : public KeyedService {
 public:
  explicit FieldInfoManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~FieldInfoManager() override;

  // Caches |info|.
  void AddFieldInfo(const FieldInfo& new_info,
                    const std::optional<FormPredictions>& predictions);

  // Retrieves field info for the given |signon_realm|.
  std::vector<FieldInfo> GetFieldInfo(const std::string& signon_realm);

  // Propagates signatures and field type received from the server.
  void ProcessServerPredictions(
      const std::map<autofill::FormSignature, FormPredictions>& predictions);

 private:
  struct FieldInfoEntry {
    // Cached field info.
    FieldInfo field_info;

    // The timer for tracking field info expiration.
    std::unique_ptr<base::OneShotTimer> timer;

    FieldInfoEntry(FieldInfo field_info,
                   std::unique_ptr<base::OneShotTimer> timer);
    ~FieldInfoEntry();
  };

  // Deletes the oldest field info entry.
  void ClearOldestFieldInfoEntry();

  // TODO(crbug.com/40277063): Reset the cache after a save prompt is accepted.
  std::deque<FieldInfoEntry> field_info_cache_;

  // Task runner used for evicting field info entries after timeout.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
