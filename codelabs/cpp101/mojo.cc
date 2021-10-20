// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "codelabs/cpp101/services/math/math_service.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/remote.h"

int main(int argc, char* argv[]) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
  mojo::core::Init();

  if (argc <= 2) {
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

  base::RunLoop run_loop;

  mojo::Remote<math::mojom::MathService> math_service;
  math::MathService math_service_impl(
      math_service.BindNewPipeAndPassReceiver());

  math_service->Divide(dividend, divisor,
                       base::BindOnce(
                           [](base::OnceClosure quit, int32_t quotient) {
                             LOG(INFO) << "Quotient: " << quotient;
                             std::move(quit).Run();
                           },
                           run_loop.QuitClosure()));

  run_loop.Run();

  return 0;
}