// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_RECORDER_FACTORY_IMPL_H_
#define COMPONENTS_METRICS_DWA_DWA_RECORDER_FACTORY_IMPL_H_

#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"

namespace metrics::dwa {

// Implements the public mojo DwaRecorderFactory interface by wrapping the
// DwaRecorder instance.
class DwaRecorderFactoryImpl : public metrics::dwa::mojom::DwaRecorderFactory {
 public:
  explicit DwaRecorderFactoryImpl(DwaRecorder* dwa_recorder);

  DwaRecorderFactoryImpl(const DwaRecorderFactoryImpl&) = delete;
  DwaRecorderFactoryImpl& operator=(const DwaRecorderFactoryImpl&) = delete;

  ~DwaRecorderFactoryImpl() override;

  // Binds the lifetime of a DwaRecorderFactory implementation to the lifetime
  // of `receiver`.
  static void Create(
      DwaRecorder* dwa_recorder,
      mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderFactory> receiver);

 private:
  // dwa::mojom::DwaRecorderFactory:
  void CreateDwaRecorder(
      mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderInterface> receiver,
      mojo::PendingRemote<metrics::dwa::mojom::DwaRecorderClientInterface>
          client_remote) override;

  // Pointer to the dwa_recorder_ singleton.
  raw_ptr<DwaRecorder> dwa_recorder_;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_RECORDER_FACTORY_IMPL_H_
