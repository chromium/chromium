// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/renderer_fuzzing/renderer_in_process_mojolpm_fuzzer.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/renderer_fuzzing/in_process_renderer_fuzzing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-mojolpm.h"
#include "third_party/blink/public/web/web_testing_support.h"

// This class fuzzes mojo interfaces exposed by the browser process to the
// renderer process using MojoLPM.
// It runs in the renderer process, and is fed testcases by the fuzzer
// running in the browser process.
// It currently uses MojoLPMGenerator in order to remove all the unnecessary
// boilerplate implied by using MojoLPM.
class RendererTestcase
    : public mojolpmgenerator::RendererInProcessMojolpmFuzzerTestcase {
 public:
  explicit RendererTestcase(
      std::unique_ptr<ProtoTestcase> testcase,
      const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
      blink::ThreadSafeBrowserInterfaceBrokerProxy*
          process_interface_broker_proxy);
  ~RendererTestcase() override;

  scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() override;
  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;
  void HandleNewBlobRegistryAction(uint32_t id,
                                   base::OnceClosure done_closure) override;

 private:
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  template <typename T>
  void NewProcessInterface(uint32_t id, base::OnceClosure done_closure);
  template <typename T>
  void NewContextInterface(uint32_t id, base::OnceClosure done_closure);

  // This is different to the "normal" MojoLPM testcase model, since we need
  // to also own the lifetime of the protobuf object, when it's normally owned
  // by libfuzzer.
  std::unique_ptr<ProtoTestcase> proto_testcase_ptr_;

  // Bindings
  [[maybe_unused]] raw_ptr<const blink::BrowserInterfaceBrokerProxy>
      context_interface_broker_proxy_;
  [[maybe_unused]] raw_ptr<blink::ThreadSafeBrowserInterfaceBrokerProxy>
      process_interface_broker_proxy_;

  SEQUENCE_CHECKER(sequence_checker_);
};

namespace {

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunnerImpl() {
  // XXX: This should be main thread? IO thread? Probably doesn't
  // actually matter.
  static scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  return fuzzer_task_runner;
}

}  // anonymous namespace

void RendererTestcase::HandleNewBlobRegistryAction(
    uint32_t id,
    base::OnceClosure done_closure) {
  NewProcessInterface<::blink::mojom::BlobRegistry>(id,
                                                    std::move(done_closure));
}

scoped_refptr<base::SequencedTaskRunner>
RendererTestcase::GetFuzzerTaskRunner() {
  return GetFuzzerTaskRunnerImpl();
}

RendererTestcase::RendererTestcase(
    std::unique_ptr<ProtoTestcase> testcase,
    const blink::BrowserInterfaceBrokerProxy* context_interface_broker_proxy,
    blink::ThreadSafeBrowserInterfaceBrokerProxy*
        process_interface_broker_proxy)
    : mojolpmgenerator::RendererInProcessMojolpmFuzzerTestcase(*testcase.get()),
      proto_testcase_ptr_(std::move(testcase)),
      context_interface_broker_proxy_(context_interface_broker_proxy),
      process_interface_broker_proxy_(process_interface_broker_proxy) {
  // RendererTestcase is created on the main thread, but the actions that
  // we want to validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RendererTestcase::~RendererTestcase() {}

void RendererTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererTestcase::SetUpOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void RendererTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&RendererTestcase::TearDownOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void RendererTestcase::SetUpOnFuzzerThread(base::OnceClosure done_closure) {
  mojolpm::GetContext()->StartTestcase();

  std::move(done_closure).Run();
}

void RendererTestcase::TearDownOnFuzzerThread(base::OnceClosure done_closure) {
  mojolpm::GetContext()->EndTestcase();

  std::move(done_closure).Run();
}

template <typename T>
void RendererTestcase::NewProcessInterface(uint32_t id,
                                           base::OnceClosure done_closure) {
  mojo::Remote<T> remote;
  mojo::GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();

  process_interface_broker_proxy_->GetInterface(std::move(receiver));
  CHECK(remote.is_bound() && remote.is_connected());

  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(done_closure).Run();
}

template <typename T>
void RendererTestcase::NewContextInterface(uint32_t id,
                                           base::OnceClosure done_closure) {
  mojo::Remote<T> remote;
  mojo::GenericPendingReceiver receiver = remote.BindNewPipeAndPassReceiver();

  context_interface_broker_proxy_->GetInterface(std::move(receiver));
  CHECK(remote.is_bound() && remote.is_connected());

  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(done_closure).Run();
}

// `RendererFuzzingAdapter` will be allocated by the internal renderer fuzzing
// mechanism. It is statically allocated, and will remain alive until the
// fuzzing process shuts down.
// Unfortunately, we cannot merge this class with `RendererTestcase`, because
// the latter needs to have a different lifetime. Indeed, it needs to be
// recreated for every fuzzing iteration, so that MojoLPM remains deterministic
// across runs for a given testcase.
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

      GetFuzzerTaskRunnerImpl()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &mojolpm::RunTestcase<RendererTestcase>,
              base::Unretained(ptr.get()), GetFuzzerTaskRunnerImpl(),
              std::move(done_closure)
                  .Then(base::OnceClosure(
                      base::DoNothingWithBoundArgs(std::move(ptr))))));
    } else {
      std::move(done_closure).Run();
    }
  }
};

REGISTER_IN_PROCESS_RENDERER_PROTO_FUZZER(RendererFuzzingAdapter);
