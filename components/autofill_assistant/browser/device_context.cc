// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/device_context.h"

namespace autofill_assistant {
Version::Version() {}
Version::Version(const Version& orig) = default;
Version::~Version() = default;

DeviceContext::DeviceContext() {}
DeviceContext::DeviceContext(const DeviceContext& orig) = default;
DeviceContext::~DeviceContext() = default;

void DeviceContext::ToProto(
    ClientContextProto_DeviceContextProto* device_context) const {
  device_context->mutable_version()->set_sdk_int(version.sdk_int);
  device_context->set_manufacturer(manufacturer);
  device_context->set_model(model);
}

}  // namespace autofill_assistant
