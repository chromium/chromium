// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/export/csv_writer.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#define CSV_EOL_SEQUENCE "\r\n"
#else
#define CSV_EOL_SEQUENCE "\n"
#endif

namespace password_manager {

class CSVWriterTest : public testing::Test {
 public:
  CSVWriterTest() = default;

  CSVWriterTest(const CSVWriterTest&) = delete;
  CSVWriterTest& operator=(const CSVWriterTest&) = delete;

  void SetUp() override {
    column_names_.push_back("foo");
    column_names_.push_back("bar");
  }

 protected:
  std::vector<std::string> column_names_;
  std::vector<std::map<std::string, std::string>> records_;
};

TEST_F(CSVWriterTest, EmptyData) {
  const char kExpectedResult[] = CSV_EOL_SEQUENCE;

  std::string result;
  WriteCSV(std::vector<std::string>(), records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, OutputArgumentIsOverwritten) {
  const char kExpectedResult[] = CSV_EOL_SEQUENCE;

  std::string result = "this text should get erased";
  WriteCSV(std::vector<std::string>(), records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, SingleColumn) {
  const char kExpectedResult[] =
      "foo" CSV_EOL_SEQUENCE "alpha" CSV_EOL_SEQUENCE "beta" CSV_EOL_SEQUENCE;

  column_names_.pop_back();
  records_.resize(2);
  records_[0]["foo"] = "alpha";
  records_[1]["foo"] = "beta";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, HeaderOnly) {
  const char kExpectedResult[] = "foo,bar" CSV_EOL_SEQUENCE;

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, HeaderAndSimpleRecords) {
  const char kExpectedResult[] =
      "foo,bar,baz" CSV_EOL_SEQUENCE "alpha,beta,gamma" CSV_EOL_SEQUENCE
      "delta,epsilon,zeta" CSV_EOL_SEQUENCE;

  column_names_.push_back("baz");

  records_.resize(2);
  records_[0]["foo"] = "alpha";
  records_[0]["bar"] = "beta";
  records_[0]["baz"] = "gamma";
  records_[1]["foo"] = "delta";
  records_[1]["bar"] = "epsilon";
  records_[1]["baz"] = "zeta";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, ExtraSpacesArePreserved) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE " alpha  beta ,  " CSV_EOL_SEQUENCE;

  records_.resize(1);
  records_[0]["foo"] = " alpha  beta ";
  records_[0]["bar"] = "  ";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, CharactersOutsideASCIIPrintableArePreservedVerbatim) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE
      "\x07\t\x0B\x1F,$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2" CSV_EOL_SEQUENCE;

  records_.resize(1);
  // Characters below 0x20: bell, horizontal tab, vertical tab, unit separator.
  records_[0]["foo"] = "\x07\t\x0B\x1F";
  // Unicode code points having 1..4 byte UTF-8 representation: dollar sign
  // (U+0024), cent sign (U+00A2), snowman (U+2603), Han character U+24B62.
  records_[0]["bar"] = "$\xc2\xa2\xe2\x98\x83\xf0\xa4\xad\xa2";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, ValueWithSeparatorsIsEnclosedInDoubleQuotes) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE "\"A\rB\",\"B\nC\"" CSV_EOL_SEQUENCE
      "\"C\r\nD\",\"D\n\"" CSV_EOL_SEQUENCE "\",\",\",,\"" CSV_EOL_SEQUENCE;

  records_.resize(3);
  records_[0]["foo"] = "A\rB";
  records_[0]["bar"] = "B\nC";
  records_[1]["foo"] = "C\r\nD";
  records_[1]["bar"] = "D\n";
  records_[2]["foo"] = ",";
  records_[2]["bar"] = ",,";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, DoubleQuotesInValueAreEscaped) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE
      "\"\"\"\",\"A\"\"B\"\"\"\"C\"" CSV_EOL_SEQUENCE;

  records_.resize(1);
  records_[0]["foo"] = "\"";
  records_[0]["bar"] = "A\"B\"\"C";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, EmptyFields) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE ",alpha" CSV_EOL_SEQUENCE
      "beta," CSV_EOL_SEQUENCE "," CSV_EOL_SEQUENCE;

  records_.resize(3);
  records_[0]["foo"] = "";
  records_[0]["bar"] = "alpha";
  records_[1]["foo"] = "beta";
  records_[1]["bar"] = "";
  records_[2]["foo"] = "";
  records_[2]["bar"] = "";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

TEST_F(CSVWriterTest, MissingValuesAreTreatedAsEmptyValues) {
  const char kExpectedResult[] =
      "foo,bar" CSV_EOL_SEQUENCE ",alpha" CSV_EOL_SEQUENCE
      "beta," CSV_EOL_SEQUENCE "," CSV_EOL_SEQUENCE;

  records_.resize(3);
  records_[0]["bar"] = "alpha";
  records_[1]["foo"] = "beta";

  std::string result;
  WriteCSV(column_names_, records_, &result);
  EXPECT_EQ(kExpectedResult, result);
}

}  // namespace password_manager

#undef CSV_EOL_SEQUENCE
