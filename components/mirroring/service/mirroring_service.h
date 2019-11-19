// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace mirroring {

class Session;

class COMPONENT_EXPORT(MIRRORING_SERVICE) MirroringService final
    : public mojom::MirroringService {
 public:
  MirroringService(mojo::PendingReceiver<mojom::MirroringService> receiver,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~MirroringService() override;

 private:
  // mojom::MirroringService implementation.
  void Start(mojom::SessionParametersPtr params,
             const gfx::Size& max_resolution,
             mojo::PendingRemote<mojom::SessionObserver> observer,
             mojo::PendingRemote<mojom::ResourceProvider> resource_provider,
             mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
             mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel)
      override;

  mojo::Receiver<mojom::MirroringService> receiver_;
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<Session> session_;  // Current mirroring session.

  DISALLOW_COPY_AND_ASSIGN(MirroringService);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
