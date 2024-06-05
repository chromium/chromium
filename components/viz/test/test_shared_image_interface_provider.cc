// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_shared_image_interface_provider.h"

#include "gpu/command_buffer/client/test_shared_image_interface.h"

namespace viz {

TestSharedImageInterfaceProvider::TestSharedImageInterfaceProvider()
    : SharedImageInterfaceProvider(nullptr),
      shared_image_interface_(
          base::MakeRefCounted<gpu::TestSharedImageInterface>()) {}

TestSharedImageInterfaceProvider::~TestSharedImageInterfaceProvider() = default;

gpu::SharedImageInterface*
TestSharedImageInterfaceProvider::GetSharedImageInterface() {
  return shared_image_interface_.get();
}

}  // namespace viz
