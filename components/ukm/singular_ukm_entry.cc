// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/singular_ukm_entry.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace ukm {

namespace {

// Browser process interface to receive the UkmEntry from remote processes.
class MojoSingularUkmInterface : public mojom::SingularUkmInterface {
 public:
  MojoSingularUkmInterface() = default;

  MojoSingularUkmInterface(const MojoSingularUkmInterface&) = delete;
  MojoSingularUkmInterface& operator=(const MojoSingularUkmInterface&) = delete;

  // When the mojo connection is lost the current UkmEntry will be recorded.
  ~MojoSingularUkmInterface() override {
    ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
    if (!entry_.is_null() && recorder) {
      recorder->AddEntry(std::move(entry_));
    }
  }

  // SingularUkmInterface:
  void Submit(mojom::UkmEntryPtr entry) override { entry_ = std::move(entry); }

 private:
  // The UkmEntry containing any submitted metrics.
  mojom::UkmEntryPtr entry_;
};

}  // namespace

void CreateSingularUkmInterface(
    mojo::PendingReceiver<mojom::SingularUkmInterface> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<MojoSingularUkmInterface>(),
                              std::move(receiver));
}

}  // namespace ukm
