// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_REQUIRED_COMPONENTS_CONTROLLER_H_
#define COMPONENTS_COMPONENT_UPDATER_REQUIRED_COMPONENTS_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace update_client {
struct CrxComponent;
struct CrxUpdateItem;
}  // namespace update_client

namespace component_updater {

struct ComponentRegistration;

class RequiredComponentsController {
 public:
  explicit RequiredComponentsController(std::vector<std::string> components);

  RequiredComponentsController(const RequiredComponentsController&) = delete;
  RequiredComponentsController& operator=(const RequiredComponentsController&) =
      delete;
  ~RequiredComponentsController();

  // Returns true if the component with the given name is required. The
  // component is required if it matches a key in |components_| map exactly or
  // with a wild card pattern.
  bool IsRequiredComponent(const std::string& name) const;

  // Requests component update if the specified component is required.
  // The caller is responsible for triggering on demand update if this happens
  // to be the case.
  bool RequestComponentUpdate(const ComponentRegistration& component);

  // Delays the caller by running a local loop until all the required components
  // are up to date. Calls LGO(FATAL) if |timeout| occurs before components are
  // ready.
  void EnsureRequiredComponentsReady(base::TimeDelta timeout);

  // Updates |crx| for a required component forcing the component update if it
  // is required.
  void ToCrxComponent(const ComponentRegistration& component,
                      update_client::CrxComponent& crx) const;

  // Called by the update client when a component makes progress. Registers
  // the component update if it is successful, or calls LOG(FATAL) if component
  // update failed.
  void OnEvent(const update_client::CrxUpdateItem& update_item);

 private:
  void CheckComponentsReady();
  std::string GetRequestedComponentNames() const;

  // Holds the required component names or patterns with * and ? wildcards.
  std::vector<std::string> components_;

  // Holds the requested components ids and names.
  base::flat_map<std::string, std::string> requested_components_;

  // Holds the up to date components names.
  base::flat_set<std::string> ready_components_;

  // Set to true when all the requested components are ready.
  bool ready_ = false;

  // Called when all the requested components are ready.
  base::OnceClosure ready_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RequiredComponentsController> weak_ptr_factory_{this};
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_REQUIRED_COMPONENTS_CONTROLLER_H_
