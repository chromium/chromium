// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/fast_pair_data_parser.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "ash/quick_pair/common/logging.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "chromeos/ash/services/quick_pair/public/mojom/fast_pair_data_parser.mojom.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

struct Environment {
  Environment()
      : task_environment(
            (base::CommandLine::Init(0, nullptr),
             TestTimeouts::Initialize(),
             base::test::TaskEnvironment::MainThreadType::DEFAULT)) {
    mojo::core::Init();
    // Disable noisy logging for fuzzing.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    // Create instance once to be reused between fuzzing rounds.
    parser = std::make_unique<ash::quick_pair::FastPairDataParser>(
        remote.BindNewPipeAndPassReceiver());
  }

  base::test::TaskEnvironment task_environment;
  mojo::Remote<ash::quick_pair::mojom::FastPairDataParser> remote;
  std::unique_ptr<ash::quick_pair::FastPairDataParser> parser;
  ash::quick_pair::ScopedDisableLoggingForTesting disable_logging;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::NoDestructor<Environment> env;
  FuzzedDataProvider fuzzed_data(data, size);

  // Does not structure the fuzzed data so that we test all possible inputs,
  // with the tradeoff that deeper code paths may not be reached.
  size_t first_vec_size = fuzzed_data.ConsumeIntegral<uint16_t>();
  std::vector<uint8_t> first_vec =
      fuzzed_data.ConsumeBytes<uint8_t>(first_vec_size);
  std::vector<uint8_t> second_vec =
      fuzzed_data.ConsumeRemainingBytes<uint8_t>();

  env->parser->GetHexModelIdFromServiceData(first_vec, base::DoNothing());

  env->parser->ParseDecryptedResponse(first_vec, second_vec, base::DoNothing());

  env->parser->ParseDecryptedPasskey(first_vec, second_vec, base::DoNothing());

  std::string second_vec_str =
      std::string(second_vec.begin(), second_vec.end());
  env->parser->ParseNotDiscoverableAdvertisement(first_vec, second_vec_str,
                                                 base::DoNothing());

  env->parser->ParseMessageStreamMessages(first_vec, base::DoNothing());

  return 0;
}
