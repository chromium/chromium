
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_IN_PROCESS_PROTO_FUZZER_H_
#define CHROME_TEST_FUZZING_IN_PROCESS_PROTO_FUZZER_H_

#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

#define DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(use_binary, arg)      \
  static void TestOneProtoInput(arg);                             \
  using FuzzerProtoType =                                         \
      protobuf_mutator::libfuzzer::macro_internal::GetFirstParam< \
          decltype(&TestOneProtoInput)>::type;                    \
  DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(use_binary, FuzzerProtoType)   \
  DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(use_binary, FuzzerProtoType) \
  DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(FuzzerProtoType)

// Register a text-based proto in process fuzzer.
// The argument should be a class implementing InProcessProtoFuzzer,
// parameterized by the <protobuf fuzz case type>
#define REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER(arg) \
  REGISTER_IN_PROCESS_FUZZER(arg)                  \
  DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(false, arg::FuzzCase testcase)
// Same as REGISTER_TEXT_PROTO_IN_PROCESS_FUZZER but uses a binary
// protobuf. May offer speed advantages at the expense of corpus
// clarity, but that's not currently clear.
#define REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(arg) \
  REGISTER_IN_PROCESS_FUZZER(arg)                    \
  DEFINE_PROTO_FUZZER_IN_PROCESS_IMPL(true, arg::FuzzCase testcase)

template <typename Proto>
class InProcessProtoFuzzer : public InProcessFuzzer {
 public:
  using FuzzCase = Proto;
  // Constructor. Optionally pass options through to the InProcessFuzzer
  // constructor.
  // NOLINTNEXTLINE(runtime/explicit)
  InProcessProtoFuzzer(InProcessFuzzerOptions options = {})
      : InProcessFuzzer(options) {}

 protected:
  int Fuzz(const uint8_t* data, size_t size) override {
    using protobuf_mutator::libfuzzer::LoadProtoInput;
    FuzzCase fuzz_case;
    if (!LoadProtoInput(false, data, size, &fuzz_case)) {
      return -1;
    }
    return Fuzz(fuzz_case);
  }

  // Execute a fuzz case using the given input.
  virtual int Fuzz(const FuzzCase& fuzz_case) = 0;
};

#endif  // CHROME_TEST_FUZZING_IN_PROCESS_PROTO_FUZZER_H_
