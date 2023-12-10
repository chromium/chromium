// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_DELEGATE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_DELEGATE_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/types/processed_value.h"

namespace segmentation_platform::processing {

class FeatureProcessorState;

// Delegate that provides inputs to the query processor that computes input and
// output features.
class InputDelegate {
 public:
  InputDelegate();
  virtual ~InputDelegate();

  InputDelegate(const InputDelegate&) = delete;
  InputDelegate& operator=(const InputDelegate&) = delete;

  // Processes the given `input`, and returns the result via `callback`. Should
  // return an error if the processing failed. On success, the number of outputs
  // in the Tensor should be equal to `input.tensor_length()`.
  using ProcessedCallback = base::OnceCallback<void(/*error=*/bool, Tensor)>;
  virtual void Process(const proto::CustomInput& input,
                       FeatureProcessorState& feature_processor_state,
                       ProcessedCallback callback) = 0;
};

// A holder that stores the list of `InputDelegate`s used by the platform.
class InputDelegateHolder {
 public:
  InputDelegateHolder();
  ~InputDelegateHolder();

  InputDelegateHolder(const InputDelegateHolder&) = delete;
  InputDelegateHolder& operator=(const InputDelegateHolder&) = delete;

  // Returns a delegate for the `policy` if available or nullptr otherwise.
  InputDelegate* GetDelegate(proto::CustomInput::FillPolicy policy);

  // Sets a delegate for the given `policy`. Overwrites any existing delegates
  // for the same `policy`
  void SetDelegate(proto::CustomInput::FillPolicy policy,
                   std::unique_ptr<InputDelegate> delegate);

 private:
  base::flat_map<proto::CustomInput::FillPolicy, std::unique_ptr<InputDelegate>>
      input_delegates_;
};

}  // namespace segmentation_platform::processing

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_PUBLIC_INPUT_DELEGATE_H_
