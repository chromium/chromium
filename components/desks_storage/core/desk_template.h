// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_H_

#include <string>

#include "base/time/time.h"

namespace sync_pb {
class WorkspaceDeskSpecifics;
}

namespace desks_storage {

// A desk template being saved. The UUID is a unique identifier for a
// template. This class is a temporary placeholder. This could be replaced
// by future ash::DeskTemplate when it is ready.
//
// TODO(crbug/1225727): remove this class.
class DeskTemplate {
 public:
  // Creates a DeskTemplate from the protobuf format.
  static std::unique_ptr<DeskTemplate> FromProto(
      const sync_pb::WorkspaceDeskSpecifics& pb_entry);

  // Creates a DeskTemplate consisting of only the required fields.
  static std::unique_ptr<DeskTemplate> FromRequiredFields(
      const std::string& uuid);

  // Creates a DeskTemplate.
  DeskTemplate(const std::string& uuid,
               const std::string& name,
               base::Time created_time);
  DeskTemplate(const DeskTemplate&) = delete;
  DeskTemplate& operator=(const DeskTemplate&) = delete;
  ~DeskTemplate();

  const std::string& uuid() const { return uuid_; }
  const std::string& name() const { return name_; }
  base::Time created_time() const { return created_time_; }

  // Returns a protobuf encoding the content of this DeskTemplate for
  // Sync.
  sync_pb::WorkspaceDeskSpecifics AsSyncProto() const;

 private:
  // The unique random id for the entry.
  std::string uuid_;
  // The name of the desk template. Might be empty.
  std::string name_;
  // The time that the desk template was created.
  base::Time created_time_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_DESK_TEMPLATE_H_