// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/preg_parser.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/policy_load_status.h"
#include "components/policy/core/common/registry_dict.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace preg_parser {
namespace {

// Preg files are relative to |kRegistryPolBaseDir|.
const char kRegistryPolBaseDir[] = "chrome/test/data/policy/gpo";
const char kRegistryPolFile[] = "parser_test/registry.pol";
const char kInvalidEncodingRegistryPolFile[] = "invalid_encoding/registry.pol";
const char kNonExistingRegistryPolFile[] = "does_not_exist.pol";

const char16_t kRegistryKey[] = u"SOFTWARE\\Policies\\Chromium";

// Check whether two RegistryDicts equal each other.
testing::AssertionResult RegistryDictEquals(const RegistryDict& a,
                                            const RegistryDict& b) {
  auto iter_key_a = a.keys().begin();
  auto iter_key_b = b.keys().begin();
  for (; iter_key_a != a.keys().end() && iter_key_b != b.keys().end();
       ++iter_key_a, ++iter_key_b) {
    if (iter_key_a->first != iter_key_b->first) {
      return testing::AssertionFailure() << "Key mismatch " << iter_key_a->first
                                         << " vs. " << iter_key_b->first;
    }
    testing::AssertionResult result =
        RegistryDictEquals(*iter_key_a->second, *iter_key_b->second);
    if (!result)
      return result;
  }
  if (iter_key_a != a.keys().end())
    return testing::AssertionFailure()
           << "key mismatch, a has extra key " << iter_key_a->first;
  if (iter_key_b != b.keys().end())
    return testing::AssertionFailure()
           << "key mismatch, b has extra key " << iter_key_b->first;

  auto iter_value_a = a.values().begin();
  auto iter_value_b = b.values().begin();
  for (; iter_value_a != a.values().end() && iter_value_b != b.values().end();
       ++iter_value_a, ++iter_value_b) {
    if (iter_value_a->first != iter_value_b->first ||
        iter_value_a->second != iter_value_b->second) {
      return testing::AssertionFailure()
             << "Value mismatch " << iter_value_a->first << "="
             << iter_value_a->second << " vs. " << iter_value_b->first << "="
             << iter_value_b->second;
    }
  }
  if (iter_value_a != a.values().end())
    return testing::AssertionFailure()
           << "Value mismatch, a has extra value " << iter_value_a->first << "="
           << iter_value_a->second;
  if (iter_value_b != b.values().end())
    return testing::AssertionFailure()
           << "Value mismatch, b has extra value " << iter_value_b->first << "="
           << iter_value_b->second;

  return testing::AssertionSuccess();
}

void SetInteger(RegistryDict* dict, const std::string& name, int value) {
  dict->SetValue(name, base::Value(value));
}

void SetString(RegistryDict* dict,
               const std::string& name,
               const std::string& value) {
  dict->SetValue(name, base::Value(value));
}

class PRegParserTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII(kRegistryPolBaseDir);
  }

  base::FilePath test_data_dir_;
};

TEST_F(PRegParserTest, TestParseFile) {
  // Prepare the test dictionary with some data so the test can check that the
  // PReg action triggers work, i.e. remove these items.
  RegistryDict dict;
  SetInteger(&dict, "DeleteValuesTest1", 1);
  SetString(&dict, "DeleteValuesTest2", "2");
  dict.SetKey("DeleteKeysTest1", std::make_unique<RegistryDict>());
  std::unique_ptr<RegistryDict> delete_keys_test(new RegistryDict());
  SetInteger(delete_keys_test.get(), "DeleteKeysTest2Entry", 1);
  dict.SetKey("DeleteKeysTest2", std::move(delete_keys_test));
  SetInteger(&dict, "DelTest", 1);
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  SetInteger(subdict.get(), "DelValsTest1", 1);
  SetString(subdict.get(), "DelValsTest2", "2");
  subdict->SetKey("DelValsTest3", std::make_unique<RegistryDict>());
  dict.SetKey("DelValsTest", std::move(subdict));

  // Run the parser.
  base::FilePath test_file(test_data_dir_.AppendASCII(kRegistryPolFile));
  PolicyLoadStatusUmaReporter status;
  ASSERT_TRUE(preg_parser::ReadFile(test_file, kRegistryKey, &dict, &status));

  // Build the expected output dictionary.
  RegistryDict expected;
  std::unique_ptr<RegistryDict> del_vals_dict(new RegistryDict());
  del_vals_dict->SetKey("DelValsTest3", std::make_unique<RegistryDict>());
  expected.SetKey("DelValsTest", std::move(del_vals_dict));
  SetInteger(&expected, "HomepageIsNewTabPage", 1);
  SetString(&expected, "HomepageLocation", "http://www.example.com");
  SetInteger(&expected, "RestoreOnStartup", 4);
  std::unique_ptr<RegistryDict> startup_urls(new RegistryDict());
  SetString(startup_urls.get(), "1", "http://www.chromium.org");
  SetString(startup_urls.get(), "2", "http://www.example.com");
  expected.SetKey("RestoreOnStartupURLs", std::move(startup_urls));
  SetInteger(&expected, "ShowHomeButton", 1);
  SetString(&expected, "Snowman", "\xE2\x98\x83");
  SetString(&expected, "Empty", "");

  EXPECT_TRUE(RegistryDictEquals(dict, expected));
}

TEST_F(PRegParserTest, SubstringRootInvalid) {
  // A root of "Aa/Bb/Cc" should not be considered a valid root for a
  // key like "Aa/Bb/C".
  base::FilePath test_file(test_data_dir_.AppendASCII(kRegistryPolFile));
  RegistryDict empty;
  PolicyLoadStatusUmaReporter status;

  // No data should be loaded for partial roots ("Aa/Bb/C").
  RegistryDict dict1;
  ASSERT_TRUE(preg_parser::ReadFile(test_file, u"SOFTWARE\\Policies\\Chro",
                                    &dict1, &status));
  EXPECT_TRUE(RegistryDictEquals(dict1, empty));

  // Safety check with kRegistryKey (dict should not be empty).
  RegistryDict dict2;
  ASSERT_TRUE(preg_parser::ReadFile(test_file, kRegistryKey, &dict2, &status));
  EXPECT_FALSE(RegistryDictEquals(dict2, empty));
}

TEST_F(PRegParserTest, RejectInvalidStrings) {
  // Tests whether strings with invalid characters are rejected.
  base::FilePath test_file(
      test_data_dir_.AppendASCII(kInvalidEncodingRegistryPolFile));
  PolicyLoadStatusUmaReporter status;
  RegistryDict dict;
  ASSERT_TRUE(preg_parser::ReadFile(test_file, kRegistryKey, &dict, &status));

  RegistryDict empty;
  EXPECT_TRUE(RegistryDictEquals(dict, empty));
}

TEST_F(PRegParserTest, LoadStatusSampling) {
  // Tests load status sampling.
  PolicyLoadStatusUmaReporter status;
  RegistryDict dict;
  base::FilePath test_file(
      test_data_dir_.AppendASCII(kNonExistingRegistryPolFile));
  ASSERT_FALSE(preg_parser::ReadFile(test_file, kRegistryKey, &dict, &status));

  PolicyLoadStatusSampler::StatusSet expected_status_set;
  expected_status_set[POLICY_LOAD_STATUS_STARTED] = true;
  expected_status_set[POLICY_LOAD_STATUS_READ_ERROR] = true;
  EXPECT_EQ(expected_status_set, status.GetStatusSet());
}

}  // namespace
}  // namespace preg_parser
}  // namespace policy
