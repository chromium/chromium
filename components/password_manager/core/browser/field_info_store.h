// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_STORE_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"

namespace password_manager {

struct FieldInfo;
class PasswordStoreConsumer;

// Interface for storing stats related to the web forms fields.
class FieldInfoStore {
 public:
  // Adds information about field. If the record for given form_signature and
  // field_signature already exists, the new one will be ignored.
  virtual void AddFieldInfo(const FieldInfo& field_info) = 0;

  // Retrieves all field info and notifies |consumer| on completion. The request
  // will be cancelled if the consumer is destroyed.
  virtual void GetAllFieldInfo(
      base::WeakPtr<PasswordStoreConsumer> consumer) = 0;

  // Removes all leaked credentials in the given date range. If |completion| is
  // not null, it will be posted to the |main_task_runner_| after deletions have
  // been completed. Should be called on the UI thread.
  virtual void RemoveFieldInfoByTime(base::Time remove_begin,
                                     base::Time remove_end,
                                     base::OnceClosure completion) = 0;

 protected:
  virtual ~FieldInfoStore() = default;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_STORE_H_
