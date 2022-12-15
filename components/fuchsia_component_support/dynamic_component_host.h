// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_DYNAMIC_COMPONENT_HOST_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_DYNAMIC_COMPONENT_HOST_H_

#include <fuchsia/component/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/strings/string_piece_forward.h"

namespace sys {
class ServiceDirectory;
}  // namespace sys

namespace fuchsia_component_support {

// Helper class used to manage a dynamic child component running in a
// collection, via the Component Framework.
class DynamicComponentHost {
 public:
  // Creates an instance `child_id` in the `collection`, running the Component
  // specified by `component_url`.
  //
  // `on_teardown` will be run if the Component fails to start, or is observed
  // to have stopped. The callback may delete the `DynamicComponentHost` before
  // returning.
  //
  // If `services` is valid then it will be bound into a sub-directory of the
  // calling component's outgoing directory "for_dynamic_component_host",
  // and offered to the component as "/svc".
  DynamicComponentHost(base::StringPiece collection,
                       base::StringPiece child_id,
                       base::StringPiece component_url,
                       base::OnceClosure on_teardown,
                       fidl::InterfaceHandle<fuchsia::io::Directory> services);

  // Deleting the Component implicitly `Destroy()`s it if necessary, without
  // causing `on_teardown_` to be run.
  ~DynamicComponentHost();

  // Requests that the underlying Component be destroyed. When the Component
  // has actually been destroyed the `on_teardown_` closure will be run.
  void Destroy();

  // Returns the directory of capabilities exposed by the Component.
  sys::ServiceDirectory& exposed() const { return *exposed_; }

 private:
  const std::string collection_;
  const std::string child_id_;

  // Run if the Component stops, or fails to start.
  // `this` may be invalid after the callback returns.
  base::OnceClosure on_teardown_;

  // Set during construction, and used to create and manage the Component.
  // Remains valid until `DestroyChild()` has been dispatched, or the
  // attempt to create the Component is observed to have failed.
  fuchsia::component::RealmPtr realm_;

  // Wraps the directory of capabilities exposed by the Component.
  std::unique_ptr<sys::ServiceDirectory> exposed_;

  // Used to monitor the Component for termination.
  fuchsia::component::BinderPtr binder_;
};

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_DYNAMIC_COMPONENT_HOST_H_
