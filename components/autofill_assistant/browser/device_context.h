// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVICE_CONTEXT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVICE_CONTEXT_H_

#include <string>
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

struct Version {
  Version();
  Version(const Version& orig);
  ~Version();

  int sdk_int;
};

// Information about the device.
struct DeviceContext {
  DeviceContext();
  DeviceContext(const DeviceContext& orig);
  ~DeviceContext();

  Version version;
  std::string manufacturer;
  std::string model;

  void ToProto(ClientContextProto_DeviceContextProto* device_context) const;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DEVICE_CONTEXT_H_
