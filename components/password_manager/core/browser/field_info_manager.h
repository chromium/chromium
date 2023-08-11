// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/keyed_service/core/keyed_service.h"

namespace password_manager {

constexpr base::TimeDelta kFieldInfoLifetime = base::Minutes(5);

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

  // The type of the field predicted by the server.
  autofill::ServerFieldType type = autofill::ServerFieldType::UNKNOWN_TYPE;

  // Signatures identifying the form and field on the server.
  autofill::FormSignature form_signature;
  autofill::FieldSignature field_signature;

  FieldInfo(int driver_id,
            autofill::FieldRendererId field_id,
            std::string signon_realm,
            std::u16string value);
  FieldInfo(const FieldInfo&);
  FieldInfo& operator=(const FieldInfo&);

  friend bool operator==(const FieldInfo& lhs, const FieldInfo& rhs) = default;
};

// TODO(crbug/1468297): Propagate server predictions to the class.
// Manages information about the last user-interacted fields, keeps
// the data and erases it once it becomes stale.
class FieldInfoManager : public KeyedService {
 public:
  explicit FieldInfoManager(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~FieldInfoManager() override;

  // Caches |info|.
  void AddFieldInfo(const FieldInfo& info);

  // Retrieves field info for the given |signon_realm|.
  std::vector<FieldInfo> GetFieldInfo(const std::string& signon_realm);

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

  // TODO(crbug/1468297): Reset the cache after a save prompt is accepted.
  std::deque<FieldInfoEntry> field_info_cache_;

  // Task runner used for evicting field info entries after timeout.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_MANAGER_H_
