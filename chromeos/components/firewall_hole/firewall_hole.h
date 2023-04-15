// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_FIREWALL_HOLE_FIREWALL_HOLE_H_
#define CHROMEOS_COMPONENTS_FIREWALL_HOLE_FIREWALL_HOLE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"

namespace chromeos {

// This class works with the Chrome OS permission broker to open a port in the
// system firewall. It is closed on destruction.
class COMPONENT_EXPORT(CHROMEOS_FIREWALL_HOLE) FirewallHole {
 public:
  enum class PortType {
    kUdp,
    kTcp,
  };

  using OpenCallback = base::OnceCallback<void(std::unique_ptr<FirewallHole>)>;

  // Opens a port on the system firewall for the given network interface (or all
  // interfaces if |interface| is ""). The hole will be closed when the object
  // provided to the callback is destroyed.
  static void Open(PortType type,
                   uint16_t port,
                   const std::string& interface,
                   OpenCallback callback);

  ~FirewallHole();

 private:
  static void PortAccessGranted(PortType type,
                                uint16_t port,
                                const std::string& interface,
                                base::ScopedFD lifeline_fd,
                                FirewallHole::OpenCallback callback,
                                bool success);

  FirewallHole(PortType type,
               uint16_t port,
               const std::string& interface,
               base::ScopedFD lifeline_fd);

  const PortType type_;
  const uint16_t port_;
  const std::string interface_;

  // A file descriptor used by firewalld to track the lifetime of this process.
  base::ScopedFD lifeline_fd_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_FIREWALL_HOLE_FIREWALL_HOLE_H_
