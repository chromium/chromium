// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DWA_DWA_RECORDER_INTERFACE_H_
#define COMPONENTS_METRICS_DWA_DWA_RECORDER_INTERFACE_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/dwa/dwa_recorder.h"
#include "components/metrics/dwa/mojom/dwa_interface.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace metrics::dwa {

class DwaRecorderInterface : public metrics::dwa::mojom::DwaRecorderInterface {
 public:
  explicit DwaRecorderInterface(DwaRecorder* dwa_recorder);

  DwaRecorderInterface(const DwaRecorderInterface&) = delete;
  DwaRecorderInterface& operator=(const DwaRecorderInterface&) = delete;

  ~DwaRecorderInterface() override;

  // Sets up the mojo receiver.
  static void Create(
      DwaRecorder* dwa_recorder,
      mojo::PendingReceiver<metrics::dwa::mojom::DwaRecorderInterface>
          receiver);

 private:
  // metrics::dwa::mojom::DwaRecorderInterface:
  void AddEntry(metrics::dwa::mojom::DwaEntryPtr entry) override;
  // Pointer to the dwa_recorder singleton.
  raw_ptr<DwaRecorder> dwa_recorder_;
};

}  // namespace metrics::dwa

#endif  // COMPONENTS_METRICS_DWA_DWA_RECORDER_INTERFACE_H_
