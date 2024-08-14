// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/escape.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/html_grammar.h"
#include "chrome/test/fuzzing/html_grammar.pb.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

// This fuzzer uses DomatoLPM to generate HTML based on an existing Domato
// rule.
class DomatoHtmlInProcessFuzzer : public InProcessFuzzer {
 public:
  using FuzzCase = testing::libfuzzer::research::domatolpm::fuzzcase;
  DomatoHtmlInProcessFuzzer() = default;

  int Fuzz(const uint8_t* data, size_t size) override;
};

DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(true, DomatoHtmlInProcessFuzzer::FuzzCase)
DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(true, DomatoHtmlInProcessFuzzer::FuzzCase)
DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(DomatoHtmlInProcessFuzzer::FuzzCase)
REGISTER_IN_PROCESS_FUZZER(DomatoHtmlInProcessFuzzer)

int DomatoHtmlInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  FuzzCase fuzz_case;
  if (!protobuf_mutator::libfuzzer::LoadProtoInput(false, data, size,
                                                   &fuzz_case)) {
    return -1;
  }
  domatolpm::Context ctx;
  CHECK(handle_fuzzcase(&ctx, fuzz_case));
  std::string_view html_string(ctx.GetBuilder()->view());
  // See
  // docs/security/url_display_guidelines/url_display_guidelines.md#url-length
  const size_t kMaxUrlLength = 2 * 1024 * 1024;
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(html_string, kUsePlus));
  if (url_string.length() > kMaxUrlLength) {
    return -1;
  }
  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
  return 0;
}
