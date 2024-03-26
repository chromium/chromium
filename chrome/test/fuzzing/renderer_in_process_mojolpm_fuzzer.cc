// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/fuzzing/renderer_in_process_mojolpm_fuzzer.h"
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-mojolpm.h"
#include "third_party/blink/public/web/web_testing_support.h"

// TODO(paulsemel): As of now, browser process code and renderer process code
// are mixed in this file. It would be nice to split this into multiple files.
// This can be done by introducing an InProcessFuzzer template for interacting
// with a renderer fuzzer. That way, the file would only contain the renderer
// fuzzer code.

// This class fuzzes the BlobRegistry mojo interface using MojoLPM.
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

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;
  void HandleNewBlobRegistryAction(uint32_t id,
                                   base::OnceClosure done_closure) override;
  scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() override;

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

// This class is used to register a RendererFuzzer. It will be reachable from
// the fuzzing IDL. It only purpose it to invoke RendererTestcase MojoLPM
// fuzzer in order to fuzz the mojo interfaces.
class MojoLPMRendererFuzzer : public RendererFuzzerBase {
 public:
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

  const char* Id() override { return "MojoLPMRendererFuzzer"; }
};

// MojoLPMInProcessFuzzer is the browser part of this fuzzer. It acts like any
// other InProcessFuzzer, except that it doesn't directly make use of the
// received testcase, but rather sends it to its renderer executed part.
// We could base this on InProcessProtoFuzzer, but this fuzzer is a
// little unusual in that it wants the proto as raw binary data to pass
// over to the renderer process.
class MojoLPMInProcessFuzzer : public InProcessFuzzer {
 public:
  using FuzzCase = RendererTestcase::ProtoTestcase;
  MojoLPMInProcessFuzzer()
      : InProcessFuzzer({
            RunLoopTimeoutBehavior::kDeclareInfiniteLoop,
            base::Seconds(180),
        }) {}

  int Fuzz(const uint8_t* data, size_t size) override;
};

int MojoLPMInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  base::span<const uint8_t> proto_contents(data, size);

  auto b64 = base::Base64Encode(proto_contents);
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(content::ExecJs(contents, content::JsReplace(R"(
      function base64ToArrayBuffer(base64) {
        var binaryString = atob(base64);
        var bytes = new Uint8Array(binaryString.length);
        for (var i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }
        return bytes.buffer;
      }
      internals.runFuzzer('MojoLPMRendererFuzzer', base64ToArrayBuffer($1));
      )",
                                                     b64)));
  return 0;
}

// This registers the InProcessFuzzer, the part that will be interacting with
// the fuzzing engine.
REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(MojoLPMInProcessFuzzer)
// This registers the renderer fuzzer, the part that will be executed in the
// renderer process.
REGISTER_RENDERER_FUZZER(MojoLPMRendererFuzzer);
