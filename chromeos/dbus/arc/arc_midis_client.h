// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ARC_ARC_MIDIS_CLIENT_H_
#define CHROMEOS_DBUS_ARC_ARC_MIDIS_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"

namespace chromeos {

// ArcMidisClient is used to pass an FD to the midis daemon for the purpose
// of setting up a Mojo channel. It is expected to be called once during browser
// initialization.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ARC) ArcMidisClient : public DBusClient {
 public:
  ArcMidisClient(const ArcMidisClient&) = delete;
  ArcMidisClient& operator=(const ArcMidisClient&) = delete;

  ~ArcMidisClient() override = default;

  // Factory function.
  static std::unique_ptr<ArcMidisClient> Create();

  // Bootstrap the Mojo connection between Chrome and the MIDI service.
  // Should pass in the child end of the Mojo pipe.
  virtual void BootstrapMojoConnection(base::ScopedFD fd,
                                       VoidDBusMethodCallback callback) = 0;

 protected:
  // Create() should be used instead.
  ArcMidisClient() = default;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ARC_ARC_MIDIS_CLIENT_H_
