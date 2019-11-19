// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLEANUP_RESULTS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLEANUP_RESULTS_IMPL_H_

#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

class EngineCleanupResultsImpl : public mojom::EngineCleanupResults {
 public:
  explicit EngineCleanupResultsImpl(
      InterfaceMetadataObserver* metadata_observer = nullptr);
  ~EngineCleanupResultsImpl() override;

  using DoneCallback = base::OnceCallback<void(uint32_t result_code)>;

  void BindToCallbacks(mojo::PendingAssociatedRemote<
                           mojom::EngineCleanupResults>* cleanup_results,
                       DoneCallback done_callback);

  // mojom::EngineCleanupResults
  void Done(uint32_t result_code) override;

 private:
  mojo::AssociatedReceiver<mojom::EngineCleanupResults> receiver_{this};
  DoneCallback done_callback_;
  InterfaceMetadataObserver* metadata_observer_ = nullptr;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLEANUP_RESULTS_IMPL_H_
