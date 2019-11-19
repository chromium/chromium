// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_
#define COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/compositing/vsync_parameter_observer.mojom.h"

namespace exo {

// Multiplexes vsync parameter updates from the display compositor to exo
// clients using the zcr_vsync_feedback_v1 protocol. Will maintain an IPC
// connection to the display compositor only when necessary.
class VSyncTimingManager : public viz::mojom::VSyncParameterObserver {
 public:
  // Will be notified about changes in vsync parameters.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                         base::TimeDelta interval) = 0;
  };

  // Used to setup IPC connection to display compositor.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void AddVSyncParameterObserver(
        mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) = 0;
  };

  explicit VSyncTimingManager(Delegate* delegate);
  ~VSyncTimingManager() override;

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  // Overridden from viz::mojom::VSyncParameterObserver:
  void OnUpdateVSyncParameters(base::TimeTicks timebase,
                               base::TimeDelta interval) override;

  void InitializeConnection();
  void MaybeInitializeConnection();
  void OnConnectionError();

  Delegate* const delegate_;

  std::vector<Observer*> observers_;

  mojo::Receiver<viz::mojom::VSyncParameterObserver> receiver_{this};

  base::WeakPtrFactory<VSyncTimingManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VSyncTimingManager);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_VSYNC_TIMING_MANAGER_H_
