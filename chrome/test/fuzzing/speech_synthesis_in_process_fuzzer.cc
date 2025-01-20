// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <string_view>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/speech_synthesis_in_process_fuzzer.pb.h"
#include "content/public/test/browser_test_utils.h"

class SpeechSynthesisInProcessFuzzer
    : public InProcessBinaryProtoFuzzer<
          test::fuzzing::speech_synthesis_fuzzing::FuzzCase> {
 public:
  SpeechSynthesisInProcessFuzzer() = default;

  base::CommandLine::StringVector GetChromiumCommandLineArguments() override;

  int Fuzz(const test::fuzzing::speech_synthesis_fuzzing::FuzzCase& fuzz_case)
      override;
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(SpeechSynthesisInProcessFuzzer)

base::CommandLine::StringVector
SpeechSynthesisInProcessFuzzer::GetChromiumCommandLineArguments() {
  return {FILE_PATH_LITERAL("--enable-speech-dispatcher")};
}

int SpeechSynthesisInProcessFuzzer::Fuzz(
    const test::fuzzing::speech_synthesis_fuzzing::FuzzCase& fuzz_case) {
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  if (!content::ExecJs(contents, content::JsReplace(
                                     R"(
      var utterance = new SpeechSynthesisUtterance($1);
      utterance.lang = $2;
      speechSynthesis.speak(utterance);
      )",
                                     fuzz_case.text(), fuzz_case.lang()))) {
    return -1;
  }

  return 0;
}
