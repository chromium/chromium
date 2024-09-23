// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "codelabs/cpp101/solutions/services/math/math_service.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  mojo::core::Init();

  if (argc < 3) {
    LOG(INFO) << argv[0] << ": missing operand";
    return -1;
  }

  int dividend = 0;
  if (!base::StringToInt(argv[1], &dividend)) {
    LOG(INFO) << argv[0] << ": invalid dividend '" << argv[1] << "'";
    return -1;
  }

  int divisor = 0;
  if (!base::StringToInt(argv[2], &divisor) || divisor == 0) {
    LOG(INFO) << argv[0] << ": invalid divisor '" << argv[2] << "'";
    return -1;
  }

  // Create a mojo remote and pass a corresponding receiver to `MathService`. In
  // a "real-world" situation the receiver and remote would typically be owned
  // by objects in different processes.

  // This process (remote)                                MathService (receiver)
  // |  -> create pipe and pass receiver ->                       bind to pipe |
  // |  -> send Divide() call through pipe ->       received message from pipe |
  // | awaiting response from pipe...                             run Divide() |
  // | result received                 <- pass result across pipe to remote <- |
  mojo::Remote<math::mojom::MathService> math_service;
  math::MathService math_service_impl(
      math_service.BindNewPipeAndPassReceiver());

  // `TestFuture` can be used to test code that return results asynchronously
  // through a callback. Prefer this to `RunLoop` in tests where possible!
  base::test::TestFuture<int32_t> future;
  math_service->Divide(dividend, divisor, future.GetCallback());

  int32_t quotient = future.Get();
  LOG(INFO) << "Quotient: " << quotient;

  return 0;
}
