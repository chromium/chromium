// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransientDeviceIds implementation.

#include "components/storage_monitor/transient_device_ids.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/storage_monitor/storage_info.h"

namespace storage_monitor {

TransientDeviceIds::TransientDeviceIds() {}

TransientDeviceIds::~TransientDeviceIds() {}

std::string TransientDeviceIds::GetTransientIdForDeviceId(
    const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!base::Contains(device_id_map_, device_id)) {
    std::string transient_id;
    do {
      transient_id = base::GenerateGUID();
    } while (base::Contains(transient_id_map_, transient_id));

    device_id_map_[device_id] = transient_id;
    transient_id_map_[transient_id] = device_id;
  }

  return device_id_map_[device_id];
}

std::string TransientDeviceIds::DeviceIdFromTransientId(
    const std::string& transient_id) const {
  auto it = transient_id_map_.find(transient_id);
  if (it == transient_id_map_.end())
    return std::string();
  return it->second;
}

}  // namespace storage_monitor
