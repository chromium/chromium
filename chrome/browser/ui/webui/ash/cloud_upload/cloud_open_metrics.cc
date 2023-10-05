// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"

namespace ash::cloud_upload {

// TODO(b/300861997): Save cloud_provider as a member.
CloudOpenMetrics::CloudOpenMetrics(CloudProvider cloud_provider) {}

CloudOpenMetrics::~CloudOpenMetrics() = default;

base::SafeRef<CloudOpenMetrics> CloudOpenMetrics::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

// For testing.
base::WeakPtr<CloudOpenMetrics> CloudOpenMetrics::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash::cloud_upload
