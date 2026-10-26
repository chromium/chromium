// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/password_manager/core/browser/import/csv_password.h"
#include "components/password_manager/core/browser/import/csv_password_sequence.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace password_manager {

namespace {

bool IsValid(CSVPassword::Status status) {
  switch (status) {
    case CSVPassword::Status::kOK:
    case CSVPassword::Status::kSyntaxError:
    case CSVPassword::Status::kSemanticError:
      return true;
  }
  return false;
}

base::FilePath GetImportSourceDir() {
  return base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
      .AppendASCII("components/password_manager/core/browser/import");
}

std::vector<std::string> ReadDictionary() {
  return fuzztest::ReadDictionaryFromFile(
      GetImportSourceDir()
          .AppendASCII("password_csv_reader_fuzzer.dict")
          .AsUTF8Unsafe());
}

std::vector<std::tuple<std::string>> ReadCorpus() {
  return fuzztest::ReadFilesFromDirectory(
      GetImportSourceDir()
          .AppendASCII("password_csv_reader_corpus")
          .AsUTF8Unsafe());
}

void ParseDoesNotCrash(std::string input) {
  CSVPasswordSequence seq(std::move(input));
  CHECK(IsValid(seq.result()))
      << "Invalid parsing result of the whole sequence: "
      << static_cast<int>(seq.result());
  CSVPassword copy;
  for (const auto& pwd : seq) {
    const CSVPassword::Status status = pwd.GetParseStatus();
    CHECK(IsValid(status)) << "Invalid parsing result of one row: "
                           << static_cast<int>(status);
    if (status == CSVPassword::Status::kOK) {
      // Copy the parsed password to access all its data members and allow the
      // ASAN to detect any corrupted memory inside.
      copy = pwd;
    }
  }
}

FUZZ_TEST(PasswordCsvReaderFuzzTest, ParseDoesNotCrash)
    .WithDomains(fuzztest::String().WithDictionary(ReadDictionary))
    .WithSeeds(ReadCorpus);

}  // namespace

}  // namespace password_manager
