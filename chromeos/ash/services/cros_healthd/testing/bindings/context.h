// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_

#include <memory>
#include <string>

namespace ash::cros_healthd::connectivity {

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

}  // namespace ash::cros_healthd::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CROS_HEALTHD_TESTING_BINDINGS_CONTEXT_H_
