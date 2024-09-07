// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/geo/subkey_requester.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/null_storage.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/src/cpp/test/testdata_source.h"

namespace autofill {
namespace {

using ::i18n::addressinput::NullStorage;
using ::i18n::addressinput::Source;
using ::i18n::addressinput::Storage;
using ::i18n::addressinput::TestdataSource;

const char kLocale[] = "CA";
const char kLanguage[] = "en";
const int kInvalidSize = -1;
// For subkeys = "AB~BC~MB~NB~NL~NT~NS~NU~ON~PE~QC~SK~YT"
const int kExpectedSubkeySize = 13;
const int kEmptySize = 0;

class SubKeyReceiver : public base::RefCountedThreadSafe<SubKeyReceiver> {
 public:
  SubKeyReceiver() : subkeys_size_(kInvalidSize) {}

  SubKeyReceiver(const SubKeyReceiver&) = delete;
  SubKeyReceiver& operator=(const SubKeyReceiver&) = delete;

  void OnSubKeysReceived(const std::vector<std::string>& subkeys_codes,
                         const std::vector<std::string>& subkeys_names) {
    subkeys_size_ = subkeys_codes.size();
  }

  int subkeys_size() const { return subkeys_size_; }

 private:
  friend class base::RefCountedThreadSafe<SubKeyReceiver>;
  ~SubKeyReceiver() = default;

  int subkeys_size_;
};

// A test subclass of the SubKeyRequesterImpl. Used to simulate rules not
// being loaded.
class TestSubKeyRequester : public SubKeyRequester {
 public:
  TestSubKeyRequester(std::unique_ptr<::i18n::addressinput::Source> source,
                      std::unique_ptr<::i18n::addressinput::Storage> storage,
                      const std::string& language)
      : SubKeyRequester(std::move(source), std::move(storage), language),
        should_load_rules_(true) {}

  TestSubKeyRequester(const TestSubKeyRequester&) = delete;
  TestSubKeyRequester& operator=(const TestSubKeyRequester&) = delete;

  ~TestSubKeyRequester() override = default;

  void ShouldLoadRules(bool should_load_rules) {
    should_load_rules_ = should_load_rules;
  }

  void LoadRulesForRegion(const std::string& region_code) override {
    if (should_load_rules_) {
      SubKeyRequester::LoadRulesForRegion(region_code);
    }
  }

 private:
  bool should_load_rules_;
};

class SubKeyRequesterTest : public testing::Test {
 public:
  SubKeyRequesterTest(const SubKeyRequesterTest&) = delete;
  SubKeyRequesterTest& operator=(const SubKeyRequesterTest&) = delete;

 protected:
  SubKeyRequesterTest() {
    base::FilePath file_path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path));
    file_path = file_path.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("libaddressinput"))
                    .Append(FILE_PATH_LITERAL("src"))
                    .Append(FILE_PATH_LITERAL("testdata"))
                    .Append(FILE_PATH_LITERAL("countryinfo.txt"));

    requester_ = std::make_unique<TestSubKeyRequester>(
        std::unique_ptr<Source>(
            new TestdataSource(true, file_path.AsUTF8Unsafe())),
        std::unique_ptr<Storage>(new NullStorage), kLanguage);
  }

  ~SubKeyRequesterTest() override = default;

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestSubKeyRequester> requester_;
};

// Tests that rules are not loaded by default.
TEST_F(SubKeyRequesterTest, AreRulesLoadedForRegion_NotLoaded) {
  EXPECT_FALSE(requester_->AreRulesLoadedForRegion(kLocale));
}

// Tests that the rules are loaded correctly.
TEST_F(SubKeyRequesterTest, AreRulesLoadedForRegion_Loaded) {
  requester_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));
}

// Tests that if the rules are loaded before the subkey request is started, the
// received subkeys will be returned to the delegate synchronously.
TEST_F(SubKeyRequesterTest, StartRequest_RulesLoaded) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Load the rules.
  requester_->LoadRulesForRegion(kLocale);
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));

  // Start the request.
  requester_->StartRegionSubKeysRequest(kLocale, 0, std::move(cb));

  // Since the rules are already loaded, the subkeys should be received
  // synchronously.
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kExpectedSubkeySize);
}

// Tests that if the rules are not loaded before the request and cannot be
// loaded after, the subkeys will not be received and the delegate will be
// notified.
TEST_F(SubKeyRequesterTest, StartRequest_RulesNotLoaded_WillNotLoad) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Make sure the rules will not be loaded in the StartRegionSubKeysRequest
  // call.
  requester_->ShouldLoadRules(false);

  // Start the normalization.
  requester_->StartRegionSubKeysRequest(kLocale, 0, std::move(cb));

  // Let the timeout execute.
  task_environment_.RunUntilIdle();

  // Since the rules are never loaded and the timeout is 0, the delegate should
  // get notified that the subkeys could not be received.
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kEmptySize);
}

// Tests that if the rules are not loaded before the call to
// StartRegionSubKeysRequest, they will be loaded in the call.
TEST_F(SubKeyRequesterTest, StartRequest_RulesNotLoaded_WillLoad) {
  scoped_refptr<SubKeyReceiver> subkey_receiver_ = new SubKeyReceiver();

  SubKeyReceiverCallback cb =
      base::BindOnce(&SubKeyReceiver::OnSubKeysReceived, subkey_receiver_);

  // Make sure the rules will not be loaded in the StartRegionSubKeysRequest
  // call.
  requester_->ShouldLoadRules(true);
  // Start the request.
  requester_->StartRegionSubKeysRequest(kLocale, 0, std::move(cb));

  // Even if the rules are not loaded before the call to
  // StartRegionSubKeysRequest, they should get loaded in the call. Since our
  // test source is synchronous, the request will happen synchronously
  // too.
  EXPECT_TRUE(requester_->AreRulesLoadedForRegion(kLocale));
  EXPECT_EQ(subkey_receiver_->subkeys_size(), kExpectedSubkeySize);
}

}  // namespace
}  // namespace autofill
