// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_
#define CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::nearby::presence {

// Implementation of the NearbyPresence mojo interface.
// This class acts as a bridge to the NearbyPresence library which is pulled in
// as a third_party dependency. It handles the translation from mojo calls to
// native callbacks and types that the library expects. This class runs in a
// sandboxed process and is called from the browser process.
class NearbyPresence : public mojom::NearbyPresence {
 public:
  // Creates a new instance of the NearbyPresence library. This will allocate
  // and initialize a new instance and hold on to the passed mojo pipes.
  // |on_disconnect| is called when either mojo interface disconnects and should
  // destroy this instamce.
  NearbyPresence(mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
                 base::OnceClosure on_disconnect);
  NearbyPresence(const NearbyPresence&) = delete;
  NearbyPresence& operator=(const NearbyPresence&) = delete;
  ~NearbyPresence() override;

 private:
  mojo::Receiver<mojom::NearbyPresence> nearby_presence_;
};

}  // namespace ash::nearby::presence

#endif  // CHROME_SERVICES_SHARING_NEARBY_NEARBY_PRESENCE_H_
