// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_STREAM_RECEIVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_STREAM_RECEIVER_H_

#include <string_view>
#include <vector>

#include "mojo/public/cpp/bindings/receiver.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace optimization_guide {

// Stream receiver that collects the stream responses and gives the complete
// response back when complete.
class OnDeviceModelStreamReceiver
    : public on_device_model::mojom::StreamingResponder {
 public:
  using ResultCallback = base::OnceCallback<void(std::string_view)>;

  explicit OnDeviceModelStreamReceiver(ResultCallback callback);
  ~OnDeviceModelStreamReceiver() override;

  mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
  BindNewPipeAndPassRemote() {
    return stream_receiver_.BindNewPipeAndPassRemote();
  }

 private:
  // on_device_model::mojom::StreamingResponder implementation:
  void OnResponse(const std::string& response_text) override;
  void OnComplete() override;

  mojo::Receiver<on_device_model::mojom::StreamingResponder> stream_receiver_{
      this};

  // The callback for which the complete stream response should be sent to.
  ResultCallback result_callback_;

  // Collects the streamed responses until the stream is complete.
  std::vector<std::string> responses_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_ON_DEVICE_MODEL_STREAM_RECEIVER_H_
