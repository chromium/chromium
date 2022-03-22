// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_
#define CHROMEOS_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_

#include <memory>
#include <string>

namespace chromeos {
namespace cros_healthd {
namespace connectivity {

class LocalState;
class RemoteState;

// Context contains the objects used by the connectivity test objects.
class Context {
 public:
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  virtual ~Context() = default;

  static std::unique_ptr<Context> Create(
      std::unique_ptr<LocalState> local_state,
      std::unique_ptr<RemoteState> remote_state);

 public:
  // The local state interface.
  virtual LocalState* local_state() = 0;
  // The remote state interface.
  virtual RemoteState* remote_state() = 0;

 protected:
  Context() = default;
};

}  // namespace connectivity
}  // namespace cros_healthd
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_
