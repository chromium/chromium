// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_SCAN_RESULTS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_SCAN_RESULTS_IMPL_H_

#include <string>

#include "base/callback.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

// An implementation of EngineScanResults which passes data through to callbacks
// that are set with BindToCallbacks.
class EngineScanResultsImpl : public mojom::EngineScanResults {
 public:
  explicit EngineScanResultsImpl(
      InterfaceMetadataObserver* metadata_observer = nullptr);
  ~EngineScanResultsImpl() override;

  using FoundUwSCallback =
      base::RepeatingCallback<void(UwSId pup_id, const PUPData::PUP& pup)>;
  using DoneCallback = base::OnceCallback<void(uint32_t result_code)>;

  void BindToCallbacks(
      mojo::PendingAssociatedRemote<mojom::EngineScanResults>* scan_results,
      FoundUwSCallback found_uws_callback,
      DoneCallback done_callback);

  // mojom::EngineScanResults

  void FoundUwS(UwSId pup_id, const PUPData::PUP& pup) override;
  void Done(uint32_t result_code) override;

 private:
  mojo::AssociatedReceiver<mojom::EngineScanResults> receiver_{this};
  FoundUwSCallback found_uws_callback_;
  DoneCallback done_callback_;
  InterfaceMetadataObserver* metadata_observer_ = nullptr;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_SCAN_RESULTS_IMPL_H_
