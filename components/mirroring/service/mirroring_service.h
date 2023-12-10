// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
#define COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace gfx {
class Size;
}

namespace mirroring {

class OpenscreenSessionHost;

class COMPONENT_EXPORT(MIRRORING_SERVICE) MirroringService final
    : public mojom::MirroringService {
 public:
  MirroringService(mojo::PendingReceiver<mojom::MirroringService> receiver,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  MirroringService(const MirroringService&) = delete;
  MirroringService& operator=(const MirroringService&) = delete;

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
  void SwitchMirroringSourceTab() override;

  void GetMirroringStats(GetMirroringStatsCallback callback) override;

  void OnDisconnect();

  // The receiver for the mirroring service API.
  mojo::Receiver<mojom::MirroringService> receiver_;

  // The IO task runner for this utility process.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // The current Open Screen session host, if any.
  std::unique_ptr<OpenscreenSessionHost> session_host_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_MIRRORING_SERVICE_H_
