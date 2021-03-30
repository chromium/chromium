// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "base/command_line.h"
#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "third_party/icu/fuzzers/fuzzer_utils.h"

#include "chrome/renderer/subresource_redirect/robots_rules_parser.h"

namespace subresource_redirect {

namespace {
class Environment {
 public:
  Environment() {
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    task_env_.emplace();  // Construct after calling TestTimeouts::Initialize().
  }

 private:
  IcuEnvironment icu;
  base::Optional<base::test::SingleThreadTaskEnvironment> task_env_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  FuzzedDataProvider provider(data, size);
  std::string rules = provider.ConsumeRandomLengthString();
  std::string url = provider.ConsumeRandomLengthString();

  base::RunLoop run_loop;

  RobotsRulesParser parser(base::TimeDelta::FromSeconds(1));
  parser.UpdateRobotsRules({std::move(rules)});
  parser.CheckRobotsRules(0, GURL(std::move(url)),
                          base::BindOnce(
                              [](base::RepeatingClosure run_loop_quit,
                                 RobotsRulesParser::CheckResult result) {
                                std::move(run_loop_quit).Run();
                              },
                              run_loop.QuitClosure()));

  run_loop.RunUntilIdle();
  return 0;
}

}  // namespace subresource_redirect
