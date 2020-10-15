// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_
#define CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_

#include <memory>

#include "chromeos/services/nearby/public/mojom/sharing.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace chromeos {
namespace nearby {

// Manages the life cycle of the Nearby utility process, which hosts
// functionality for both Nearby Connections and Nearby Share.
class NearbyProcessManager : public KeyedService {
 public:
  class NearbyProcessReference {
   public:
    virtual ~NearbyProcessReference() = default;
    virtual const mojo::SharedRemote<
        location::nearby::connections::mojom::NearbyConnections>&
    GetNearbyConnections() const = 0;
    virtual const mojo::SharedRemote<sharing::mojom::NearbySharingDecoder>&
    GetNearbySharingDecoder() const = 0;
  };

  ~NearbyProcessManager() override = default;

  // Returns a reference which allows clients invoke functions implemented by
  // the Nearby utility process. If at least one NearbyProcessReference is held,
  // NearbyProcessManager attempts to keep the Nearby utility process alive.
  //
  // Note that it is possible that the Nearby process could crash and shut down
  // while a NearbyReference is still held; if this occurs,
  // |on_process_stopped_callback| will be invoked, and the client should no
  // longer use the invalidated NearbyReference.
  //
  // Clients should delete their NearbyProcessReference object when they are no
  // longer using it; when there are no remaining NearbyProcessReference
  // objects, NearbyProcessManager shuts down the utility process. Note that
  // once clients delete the returned NearbyProcessReference, they will no
  // longer receive a callback once the process has stopped.
  //
  // Note: This function returns null if the user session is initializing or
  // shutting down.
  virtual std::unique_ptr<NearbyProcessReference> GetNearbyProcessReference(
      base::OnceClosure on_process_stopped_callback) = 0;
};

}  // namespace nearby
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NEARBY_PUBLIC_CPP_NEARBY_PROCESS_MANAGER_H_
