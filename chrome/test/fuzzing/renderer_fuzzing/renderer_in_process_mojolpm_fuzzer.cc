// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "chrome/test/fuzzing/renderer_fuzzing/in_process_renderer_fuzzing.h"
#include "chrome/test/fuzzing/renderer_fuzzing/testcase.h"
#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"

// `RendererFuzzingAdapter` will be allocated by the internal renderer fuzzing
// mechanism. It is statically allocated, and will remain alive until the
// fuzzing process shuts down.
// This interacts with the generated RendererTestcase MojoLPM fuzzer.
class RendererFuzzingAdapter : public RendererFuzzerBase {
 public:
  using FuzzCase = RendererTestcase::ProtoTestcase;
  const char* Id() override { return "MojoLPMRendererFuzzer"; }
  void Run(
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy,
      std::vector<uint8_t>&& input,
      base::OnceClosure done_closure) override {
    auto proto_testcase_ptr =
        std::make_unique<RendererTestcase::ProtoTestcase>();
    if (proto_testcase_ptr->ParseFromArray(input.data(), input.size())) {
      auto ptr = std::make_unique<RendererTestcase>(
          std::move(proto_testcase_ptr), context_interface_broker_proxy,
          process_interface_broker_proxy);

      ptr->GetFuzzerTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &mojolpm::RunTestcase<RendererTestcase>,
              base::Unretained(ptr.get()), ptr->GetFuzzerTaskRunner(),
              std::move(done_closure)
                  .Then(base::OnceClosure(
                      base::DoNothingWithBoundArgs(std::move(ptr))))));
    } else {
      std::move(done_closure).Run();
    }
  }
};

REGISTER_IN_PROCESS_RENDERER_PROTO_FUZZER(RendererFuzzingAdapter);
