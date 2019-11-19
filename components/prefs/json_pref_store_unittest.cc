// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/json_pref_store.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_samples.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store_unittest.h"
#include "components/prefs/pref_filter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

const char kHomePage[] = "homepage";

const char kReadJson[] =
    "{\n"
    "  \"homepage\": \"http://www.cnn.com\",\n"
    "  \"some_directory\": \"/usr/local/\",\n"
    "  \"tabs\": {\n"
    "    \"new_windows_in_tabs\": true,\n"
    "    \"max_tabs\": 20\n"
    "  }\n"
    "}";

const char kInvalidJson[] = "!@#$%^&";

// Expected output for tests using RunBasicJsonPrefStoreTest().
const char kWriteGolden[] =
    "{\"homepage\":\"http://www.cnn.com\","
     "\"long_int\":{\"pref\":\"214748364842\"},"
     "\"some_directory\":\"/usr/sbin/\","
     "\"tabs\":{\"max_tabs\":10,\"new_windows_in_tabs\":false}}";

// A PrefFilter that will intercept all calls to FilterOnLoad() and hold on
// to the |prefs| until explicitly asked to release them.
class InterceptingPrefFilter : public PrefFilter {
 public:
  InterceptingPrefFilter();
  InterceptingPrefFilter(OnWriteCallbackPair callback_pair);
  ~InterceptingPrefFilter() override;

  // PrefFilter implementation:
  void FilterOnLoad(
      PostFilterOnLoadCallback post_filter_on_load_callback,
      std::unique_ptr<base::DictionaryValue> pref_store_contents) override;
  void FilterUpdate(const std::string& path) override {}
  OnWriteCallbackPair FilterSerializeData(
      base::DictionaryValue* pref_store_contents) override {
    return std::move(on_write_callback_pair_);
  }
  void OnStoreDeletionFromDisk() override {}

  bool has_intercepted_prefs() const { return intercepted_prefs_ != nullptr; }

  // Finalize an intercepted read, handing |intercepted_prefs_| back to its
  // JsonPrefStore.
  void ReleasePrefs();

 private:
  PostFilterOnLoadCallback post_filter_on_load_callback_;
  std::unique_ptr<base::DictionaryValue> intercepted_prefs_;
  OnWriteCallbackPair on_write_callback_pair_;

  DISALLOW_COPY_AND_ASSIGN(InterceptingPrefFilter);
};

InterceptingPrefFilter::InterceptingPrefFilter() {}

InterceptingPrefFilter::InterceptingPrefFilter(
    OnWriteCallbackPair callback_pair) {
  on_write_callback_pair_ = std::move(callback_pair);
}

InterceptingPrefFilter::~InterceptingPrefFilter() {}

void InterceptingPrefFilter::FilterOnLoad(
    PostFilterOnLoadCallback post_filter_on_load_callback,
    std::unique_ptr<base::DictionaryValue> pref_store_contents) {
  post_filter_on_load_callback_ = std::move(post_filter_on_load_callback);
  intercepted_prefs_ = std::move(pref_store_contents);
}

void InterceptingPrefFilter::ReleasePrefs() {
  EXPECT_FALSE(post_filter_on_load_callback_.is_null());
  std::move(post_filter_on_load_callback_)
      .Run(std::move(intercepted_prefs_), false);
}

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void (const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void (bool));
};

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  MOCK_METHOD1(OnError, void(PersistentPrefStore::PrefReadError));
};

enum class CommitPendingWriteMode {
  // Basic mode.
  WITHOUT_CALLBACK,
  // With reply callback.
  WITH_CALLBACK,
  // With synchronous notify callback (synchronous after the write -- shouldn't
  // require pumping messages to observe).
  WITH_SYNCHRONOUS_CALLBACK,
};

base::test::TaskEnvironment::ThreadPoolExecutionMode GetExecutionMode(
    CommitPendingWriteMode commit_mode) {
  switch (commit_mode) {
    case CommitPendingWriteMode::WITHOUT_CALLBACK:
      FALLTHROUGH;
    case CommitPendingWriteMode::WITH_CALLBACK:
      return base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED;
    case CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK:
      // Synchronous callbacks require async tasks to run on their own.
      return base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC;
  }
}

void CommitPendingWrite(JsonPrefStore* pref_store,
                        CommitPendingWriteMode commit_pending_write_mode,
                        base::test::TaskEnvironment* task_environment) {
  switch (commit_pending_write_mode) {
    case CommitPendingWriteMode::WITHOUT_CALLBACK: {
      pref_store->CommitPendingWrite();
      task_environment->RunUntilIdle();
      break;
    }
    case CommitPendingWriteMode::WITH_CALLBACK: {
      TestCommitPendingWriteWithCallback(pref_store, task_environment);
      break;
    }
    case CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK: {
      base::WaitableEvent written;
      pref_store->CommitPendingWrite(
          base::OnceClosure(),
          base::BindOnce(&base::WaitableEvent::Signal, Unretained(&written)));
      written.Wait();
      break;
    }
  }
}

class JsonPrefStoreTest
    : public testing::TestWithParam<CommitPendingWriteMode> {
 public:
  JsonPrefStoreTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::DEFAULT,
                          GetExecutionMode(GetParam())) {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStoreTest);
};

}  // namespace

// Test fallback behavior for a nonexistent file.
TEST_P(JsonPrefStoreTest, NonExistentFile) {
  base::FilePath bogus_input_file = temp_dir_.GetPath().AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(bogus_input_file);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NO_FILE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
}

// Test fallback behavior for an invalid file.
TEST_P(JsonPrefStoreTest, InvalidFile) {
  base::FilePath invalid_file = temp_dir_.GetPath().AppendASCII("invalid.json");
  ASSERT_LT(0, base::WriteFile(invalid_file, kInvalidJson,
                               base::size(kInvalidJson) - 1));

  auto pref_store = base::MakeRefCounted<JsonPrefStore>(invalid_file);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The file should have been moved aside.
  EXPECT_FALSE(PathExists(invalid_file));
  base::FilePath moved_aside = temp_dir_.GetPath().AppendASCII("invalid.bad");
  EXPECT_TRUE(PathExists(moved_aside));

  std::string moved_aside_contents;
  ASSERT_TRUE(base::ReadFileToString(moved_aside, &moved_aside_contents));
  EXPECT_EQ(kInvalidJson, moved_aside_contents);
}

// This function is used to avoid code duplication while testing synchronous
// and asynchronous version of the JsonPrefStore loading. It validates that the
// given output file's contents matches kWriteGolden.
void RunBasicJsonPrefStoreTest(JsonPrefStore* pref_store,
                               const base::FilePath& output_file,
                               CommitPendingWriteMode commit_pending_write_mode,
                               base::test::TaskEnvironment* task_environment) {
  const char kNewWindowsInTabs[] = "tabs.new_windows_in_tabs";
  const char kMaxTabs[] = "tabs.max_tabs";
  const char kLongIntPref[] = "long_int.pref";

  std::string cnn("http://www.cnn.com");

  const Value* actual;
  EXPECT_TRUE(pref_store->GetValue(kHomePage, &actual));
  std::string string_value;
  EXPECT_TRUE(actual->GetAsString(&string_value));
  EXPECT_EQ(cnn, string_value);

  const char kSomeDirectory[] = "some_directory";

  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  base::FilePath::StringType path;
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("/usr/local/")), path);
  base::FilePath some_path(FILE_PATH_LITERAL("/usr/sbin/"));

  pref_store->SetValue(kSomeDirectory,
                       std::make_unique<Value>(some_path.value()),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(pref_store->GetValue(kSomeDirectory, &actual));
  EXPECT_TRUE(actual->GetAsString(&path));
  EXPECT_EQ(some_path.value(), path);

  // Test reading some other data types from sub-dictionaries.
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  bool boolean = false;
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_TRUE(boolean);

  pref_store->SetValue(kNewWindowsInTabs, std::make_unique<Value>(false),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(pref_store->GetValue(kNewWindowsInTabs, &actual));
  EXPECT_TRUE(actual->GetAsBoolean(&boolean));
  EXPECT_FALSE(boolean);

  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  int integer = 0;
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(20, integer);
  pref_store->SetValue(kMaxTabs, std::make_unique<Value>(10),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(pref_store->GetValue(kMaxTabs, &actual));
  EXPECT_TRUE(actual->GetAsInteger(&integer));
  EXPECT_EQ(10, integer);

  pref_store->SetValue(
      kLongIntPref,
      std::make_unique<Value>(base::NumberToString(214748364842LL)),
      WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  EXPECT_TRUE(pref_store->GetValue(kLongIntPref, &actual));
  EXPECT_TRUE(actual->GetAsString(&string_value));
  int64_t value;
  base::StringToInt64(string_value, &value);
  EXPECT_EQ(214748364842LL, value);

  // Serialize and compare to expected output.
  CommitPendingWrite(pref_store, commit_pending_write_mode, task_environment);

  std::string output_contents;
  ASSERT_TRUE(base::ReadFileToString(output_file, &output_contents));
  EXPECT_EQ(kWriteGolden, output_contents);
  ASSERT_TRUE(base::DeleteFile(output_file, false));
}

TEST_P(JsonPrefStoreTest, Basic) {
  base::FilePath input_file = temp_dir_.GetPath().AppendASCII("write.json");
  ASSERT_LT(0,
            base::WriteFile(input_file, kReadJson, base::size(kReadJson) - 1));

  // Test that the persistent value can be loaded.
  ASSERT_TRUE(PathExists(input_file));
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(input_file);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());
  EXPECT_TRUE(pref_store->IsInitializationComplete());

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(pref_store.get(), input_file, GetParam(),
                            &task_environment_);
}

TEST_P(JsonPrefStoreTest, BasicAsync) {
  base::FilePath input_file = temp_dir_.GetPath().AppendASCII("write.json");
  ASSERT_LT(0,
            base::WriteFile(input_file, kReadJson, base::size(kReadJson) - 1));

  // Test that the persistent value can be loaded.
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(input_file);

  {
    MockPrefStoreObserver mock_observer;
    pref_store->AddObserver(&mock_observer);

    MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    EXPECT_CALL(*mock_error_delegate,
                OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    task_environment_.RunUntilIdle();
    pref_store->RemoveObserver(&mock_observer);

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_TRUE(pref_store->IsInitializationComplete());
  }

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(pref_store.get(), input_file, GetParam(),
                            &task_environment_);
}

TEST_P(JsonPrefStoreTest, PreserveEmptyValues) {
  FilePath pref_file = temp_dir_.GetPath().AppendASCII("empty_values.json");

  auto pref_store = base::MakeRefCounted<JsonPrefStore>(pref_file);

  // Set some keys with empty values.
  pref_store->SetValue("list", std::make_unique<base::ListValue>(),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  pref_store->SetValue("dict", std::make_unique<base::DictionaryValue>(),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  // Write to file.
  CommitPendingWrite(pref_store.get(), GetParam(), &task_environment_);

  // Reload.
  pref_store = base::MakeRefCounted<JsonPrefStore>(pref_file);
  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  ASSERT_FALSE(pref_store->ReadOnly());

  // Check values.
  const Value* result = nullptr;
  EXPECT_TRUE(pref_store->GetValue("list", &result));
  EXPECT_TRUE(ListValue().Equals(result));
  EXPECT_TRUE(pref_store->GetValue("dict", &result));
  EXPECT_TRUE(DictionaryValue().Equals(result));
}

// This test is just documenting some potentially non-obvious behavior. It
// shouldn't be taken as normative.
TEST_P(JsonPrefStoreTest, RemoveClearsEmptyParent) {
  FilePath pref_file = temp_dir_.GetPath().AppendASCII("empty_values.json");

  auto pref_store = base::MakeRefCounted<JsonPrefStore>(pref_file);

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("key", "value");
  pref_store->SetValue("dict", std::move(dict),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  pref_store->RemoveValue("dict.key",
                          WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);

  const base::Value* retrieved_dict = nullptr;
  bool has_dict = pref_store->GetValue("dict", &retrieved_dict);
  EXPECT_FALSE(has_dict);
}

// Tests asynchronous reading of the file when there is no file.
TEST_P(JsonPrefStoreTest, AsyncNonExistingFile) {
  base::FilePath bogus_input_file = temp_dir_.GetPath().AppendASCII("read.txt");
  ASSERT_FALSE(PathExists(bogus_input_file));
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(bogus_input_file);
  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);

  MockReadErrorDelegate *mock_error_delegate = new MockReadErrorDelegate;
  pref_store->ReadPrefsAsync(mock_error_delegate);

  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  EXPECT_CALL(*mock_error_delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_NO_FILE)).Times(1);
  task_environment_.RunUntilIdle();
  pref_store->RemoveObserver(&mock_observer);

  EXPECT_FALSE(pref_store->ReadOnly());
}

TEST_P(JsonPrefStoreTest, ReadWithInterceptor) {
  base::FilePath input_file = temp_dir_.GetPath().AppendASCII("write.json");
  ASSERT_LT(0,
            base::WriteFile(input_file, kReadJson, base::size(kReadJson) - 1));

  std::unique_ptr<InterceptingPrefFilter> intercepting_pref_filter(
      new InterceptingPrefFilter());
  InterceptingPrefFilter* raw_intercepting_pref_filter_ =
      intercepting_pref_filter.get();
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(
      input_file, std::move(intercepting_pref_filter));

  ASSERT_EQ(PersistentPrefStore::PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE,
            pref_store->ReadPrefs());
  EXPECT_FALSE(pref_store->ReadOnly());

  // The store shouldn't be considered initialized until the interceptor
  // returns.
  EXPECT_TRUE(raw_intercepting_pref_filter_->has_intercepted_prefs());
  EXPECT_FALSE(pref_store->IsInitializationComplete());
  EXPECT_FALSE(pref_store->GetValue(kHomePage, nullptr));

  raw_intercepting_pref_filter_->ReleasePrefs();

  EXPECT_FALSE(raw_intercepting_pref_filter_->has_intercepted_prefs());
  EXPECT_TRUE(pref_store->IsInitializationComplete());
  EXPECT_TRUE(pref_store->GetValue(kHomePage, nullptr));

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(pref_store.get(), input_file, GetParam(),
                            &task_environment_);
}

TEST_P(JsonPrefStoreTest, ReadAsyncWithInterceptor) {
  base::FilePath input_file = temp_dir_.GetPath().AppendASCII("write.json");
  ASSERT_LT(0,
            base::WriteFile(input_file, kReadJson, base::size(kReadJson) - 1));

  std::unique_ptr<InterceptingPrefFilter> intercepting_pref_filter(
      new InterceptingPrefFilter());
  InterceptingPrefFilter* raw_intercepting_pref_filter_ =
      intercepting_pref_filter.get();
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(
      input_file, std::move(intercepting_pref_filter));

  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);

  // Ownership of the |mock_error_delegate| is handed to the |pref_store| below.
  MockReadErrorDelegate* mock_error_delegate = new MockReadErrorDelegate;

  {
    pref_store->ReadPrefsAsync(mock_error_delegate);

    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(0);
    // EXPECT_CALL(*mock_error_delegate,
    //             OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);
    task_environment_.RunUntilIdle();

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_TRUE(raw_intercepting_pref_filter_->has_intercepted_prefs());
    EXPECT_FALSE(pref_store->IsInitializationComplete());
    EXPECT_FALSE(pref_store->GetValue(kHomePage, nullptr));
  }

  {
    EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
    // EXPECT_CALL(*mock_error_delegate,
    //             OnError(PersistentPrefStore::PREF_READ_ERROR_NONE)).Times(0);

    raw_intercepting_pref_filter_->ReleasePrefs();

    EXPECT_FALSE(pref_store->ReadOnly());
    EXPECT_FALSE(raw_intercepting_pref_filter_->has_intercepted_prefs());
    EXPECT_TRUE(pref_store->IsInitializationComplete());
    EXPECT_TRUE(pref_store->GetValue(kHomePage, nullptr));
  }

  pref_store->RemoveObserver(&mock_observer);

  // The JSON file looks like this:
  // {
  //   "homepage": "http://www.cnn.com",
  //   "some_directory": "/usr/local/",
  //   "tabs": {
  //     "new_windows_in_tabs": true,
  //     "max_tabs": 20
  //   }
  // }

  RunBasicJsonPrefStoreTest(pref_store.get(), input_file, GetParam(),
                            &task_environment_);
}

INSTANTIATE_TEST_SUITE_P(
    WithoutCallback,
    JsonPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITHOUT_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithCallback,
    JsonPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITH_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithSynchronousCallback,
    JsonPrefStoreTest,
    ::testing::Values(CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK));

class JsonPrefStoreLossyWriteTest : public JsonPrefStoreTest {
 public:
  JsonPrefStoreLossyWriteTest() = default;

 protected:
  void SetUp() override {
    JsonPrefStoreTest::SetUp();
    test_file_ = temp_dir_.GetPath().AppendASCII("test.json");
  }

  scoped_refptr<JsonPrefStore> CreatePrefStore() {
    return base::MakeRefCounted<JsonPrefStore>(test_file_);
  }

  // Return the ImportantFileWriter for a given JsonPrefStore.
  ImportantFileWriter* GetImportantFileWriter(JsonPrefStore* pref_store) {
    return &(pref_store->writer_);
  }

  // Get the contents of kTestFile. Pumps the message loop before returning the
  // result.
  std::string GetTestFileContents() {
    task_environment_.RunUntilIdle();
    std::string file_contents;
    ReadFileToString(test_file_, &file_contents);
    return file_contents;
  }

 private:
  base::FilePath test_file_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStoreLossyWriteTest);
};

TEST_P(JsonPrefStoreLossyWriteTest, LossyWriteBasic) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  // Set a normal pref and check that it gets scheduled to be written.
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->SetValue("normal", std::make_unique<base::Value>("normal"),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  ASSERT_TRUE(file_writer->HasPendingWrite());
  file_writer->DoScheduledWrite();
  ASSERT_EQ("{\"normal\":\"normal\"}", GetTestFileContents());
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // Set a lossy pref and check that it is not scheduled to be written.
  // SetValue/RemoveValue.
  pref_store->SetValue("lossy", std::make_unique<base::Value>("lossy"),
                       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->RemoveValue("lossy", WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // SetValueSilently/RemoveValueSilently.
  pref_store->SetValueSilently("lossy", std::make_unique<base::Value>("lossy"),
                               WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->RemoveValueSilently("lossy",
                                  WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // ReportValueChanged.
  pref_store->SetValue("lossy", std::make_unique<base::Value>("lossy"),
                       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->ReportValueChanged("lossy",
                                 WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // Call CommitPendingWrite and check that the lossy pref and the normal pref
  // are there with the last values set above.
  pref_store->CommitPendingWrite(base::OnceClosure());
  ASSERT_FALSE(file_writer->HasPendingWrite());
  ASSERT_EQ("{\"lossy\":\"lossy\",\"normal\":\"normal\"}",
            GetTestFileContents());
}

TEST_P(JsonPrefStoreLossyWriteTest, LossyWriteMixedLossyFirst) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  // Set a lossy pref and check that it is not scheduled to be written.
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->SetValue("lossy", std::make_unique<base::Value>("lossy"),
                       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // Set a normal pref and check that it is scheduled to be written.
  pref_store->SetValue("normal", std::make_unique<base::Value>("normal"),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Call DoScheduledWrite and check both prefs get written.
  file_writer->DoScheduledWrite();
  ASSERT_EQ("{\"lossy\":\"lossy\",\"normal\":\"normal\"}",
            GetTestFileContents());
  ASSERT_FALSE(file_writer->HasPendingWrite());
}

TEST_P(JsonPrefStoreLossyWriteTest, LossyWriteMixedLossySecond) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  // Set a normal pref and check that it is scheduled to be written.
  ASSERT_FALSE(file_writer->HasPendingWrite());
  pref_store->SetValue("normal", std::make_unique<base::Value>("normal"),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Set a lossy pref and check that the write is still scheduled.
  pref_store->SetValue("lossy", std::make_unique<base::Value>("lossy"),
                       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Call DoScheduledWrite and check both prefs get written.
  file_writer->DoScheduledWrite();
  ASSERT_EQ("{\"lossy\":\"lossy\",\"normal\":\"normal\"}",
            GetTestFileContents());
  ASSERT_FALSE(file_writer->HasPendingWrite());
}

TEST_P(JsonPrefStoreLossyWriteTest, ScheduleLossyWrite) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  // Set a lossy pref and check that it is not scheduled to be written.
  pref_store->SetValue("lossy", std::make_unique<base::Value>("lossy"),
                       WriteablePrefStore::LOSSY_PREF_WRITE_FLAG);
  ASSERT_FALSE(file_writer->HasPendingWrite());

  // Schedule pending lossy writes and check that it is scheduled.
  pref_store->SchedulePendingLossyWrites();
  ASSERT_TRUE(file_writer->HasPendingWrite());

  // Call CommitPendingWrite and check that the lossy pref is there with the
  // last value set above.
  pref_store->CommitPendingWrite(base::OnceClosure());
  ASSERT_FALSE(file_writer->HasPendingWrite());
  ASSERT_EQ("{\"lossy\":\"lossy\"}", GetTestFileContents());
}

INSTANTIATE_TEST_SUITE_P(
    WithoutCallback,
    JsonPrefStoreLossyWriteTest,
    ::testing::Values(CommitPendingWriteMode::WITHOUT_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithReply,
    JsonPrefStoreLossyWriteTest,
    ::testing::Values(CommitPendingWriteMode::WITH_CALLBACK));
INSTANTIATE_TEST_SUITE_P(
    WithNotify,
    JsonPrefStoreLossyWriteTest,
    ::testing::Values(CommitPendingWriteMode::WITH_SYNCHRONOUS_CALLBACK));

class SuccessfulWriteReplyObserver {
 public:
  SuccessfulWriteReplyObserver() = default;

  // Returns true if a successful write was observed via on_successful_write()
  // and resets the observation state to false regardless.
  bool GetAndResetObservationState() {
    bool was_successful_write_observed = successful_write_reply_observed_;
    successful_write_reply_observed_ = false;
    return was_successful_write_observed;
  }

  // Register OnWrite() to be called on the next write of |json_pref_store|.
  void ObserveNextWriteCallback(JsonPrefStore* json_pref_store);

  void OnSuccessfulWrite() {
    EXPECT_FALSE(successful_write_reply_observed_);
    successful_write_reply_observed_ = true;
  }

 private:
  bool successful_write_reply_observed_ = false;

  DISALLOW_COPY_AND_ASSIGN(SuccessfulWriteReplyObserver);
};

void SuccessfulWriteReplyObserver::ObserveNextWriteCallback(
    JsonPrefStore* json_pref_store) {
  json_pref_store->RegisterOnNextSuccessfulWriteReply(
      base::BindOnce(&SuccessfulWriteReplyObserver::OnSuccessfulWrite,
                     base::Unretained(this)));
}

enum WriteCallbackObservationState {
  NOT_CALLED,
  CALLED_WITH_ERROR,
  CALLED_WITH_SUCCESS,
};

class WriteCallbacksObserver {
 public:
  WriteCallbacksObserver() = default;

  // Register OnWrite() to be called on the next write of |json_pref_store|.
  void ObserveNextWriteCallback(JsonPrefStore* json_pref_store);

  // Returns whether OnPreWrite() was called, and resets the observation state
  // to false.
  bool GetAndResetPreWriteObservationState();

  // Returns the |WriteCallbackObservationState| which was observed, then resets
  // it to |NOT_CALLED|.
  WriteCallbackObservationState GetAndResetPostWriteObservationState();

  JsonPrefStore::OnWriteCallbackPair GetCallbackPair() {
    return std::make_pair(base::BindOnce(&WriteCallbacksObserver::OnPreWrite,
                                         base::Unretained(this)),
                          base::BindOnce(&WriteCallbacksObserver::OnPostWrite,
                                         base::Unretained(this)));
  }

  void OnPreWrite() {
    EXPECT_FALSE(pre_write_called_);
    pre_write_called_ = true;
  }

  void OnPostWrite(bool success) {
    EXPECT_EQ(NOT_CALLED, post_write_observation_state_);
    post_write_observation_state_ =
        success ? CALLED_WITH_SUCCESS : CALLED_WITH_ERROR;
  }

 private:
  bool pre_write_called_ = false;
  WriteCallbackObservationState post_write_observation_state_ = NOT_CALLED;

  DISALLOW_COPY_AND_ASSIGN(WriteCallbacksObserver);
};

void WriteCallbacksObserver::ObserveNextWriteCallback(JsonPrefStore* writer) {
  writer->RegisterOnNextWriteSynchronousCallbacks(GetCallbackPair());
}

bool WriteCallbacksObserver::GetAndResetPreWriteObservationState() {
  bool observation_state = pre_write_called_;
  pre_write_called_ = false;
  return observation_state;
}

WriteCallbackObservationState
WriteCallbacksObserver::GetAndResetPostWriteObservationState() {
  WriteCallbackObservationState state = post_write_observation_state_;
  pre_write_called_ = false;
  post_write_observation_state_ = NOT_CALLED;
  return state;
}

class JsonPrefStoreCallbackTest : public testing::Test {
 public:
  JsonPrefStoreCallbackTest() = default;

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_file_ = temp_dir_.GetPath().AppendASCII("test.json");
  }

  scoped_refptr<JsonPrefStore> CreatePrefStore() {
    return base::MakeRefCounted<JsonPrefStore>(test_file_);
  }

  // Return the ImportantFileWriter for a given JsonPrefStore.
  ImportantFileWriter* GetImportantFileWriter(JsonPrefStore* pref_store) {
    return &(pref_store->writer_);
  }

  void TriggerFakeWriteForCallback(JsonPrefStore* pref_store, bool success) {
    JsonPrefStore::PostWriteCallback(
        base::BindOnce(&JsonPrefStore::RunOrScheduleNextSuccessfulWriteCallback,
                       pref_store->AsWeakPtr()),
        base::BindOnce(&WriteCallbacksObserver::OnPostWrite,
                       base::Unretained(&write_callback_observer_)),
        base::SequencedTaskRunnerHandle::Get(), success);
  }

  SuccessfulWriteReplyObserver successful_write_reply_observer_;
  WriteCallbacksObserver write_callback_observer_;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

  base::ScopedTempDir temp_dir_;

 private:
  base::FilePath test_file_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStoreCallbackTest);
};

TEST_F(JsonPrefStoreCallbackTest, TestSerializeDataCallbacks) {
  base::FilePath input_file = temp_dir_.GetPath().AppendASCII("write.json");
  ASSERT_LT(0,
            base::WriteFile(input_file, kReadJson, base::size(kReadJson) - 1));

  std::unique_ptr<InterceptingPrefFilter> intercepting_pref_filter(
      new InterceptingPrefFilter(write_callback_observer_.GetCallbackPair()));
  auto pref_store = base::MakeRefCounted<JsonPrefStore>(
      input_file, std::move(intercepting_pref_filter));
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  EXPECT_EQ(NOT_CALLED,
            write_callback_observer_.GetAndResetPostWriteObservationState());
  pref_store->SetValue("normal", std::make_unique<base::Value>("normal"),
                       WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
  file_writer->DoScheduledWrite();

  // The observer should not be invoked right away.
  EXPECT_FALSE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(NOT_CALLED,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  task_environment_.RunUntilIdle();

  EXPECT_TRUE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());
}

TEST_F(JsonPrefStoreCallbackTest, TestPostWriteCallbacks) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());

  // Test RegisterOnNextWriteSynchronousCallbacks after
  // RegisterOnNextSuccessfulWriteReply.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  write_callback_observer_.ObserveNextWriteCallback(pref_store.get());
  file_writer->WriteNow(std::make_unique<std::string>("foo"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_TRUE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Test RegisterOnNextSuccessfulWriteReply after
  // RegisterOnNextWriteSynchronousCallbacks.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  write_callback_observer_.ObserveNextWriteCallback(pref_store.get());
  file_writer->WriteNow(std::make_unique<std::string>("foo"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_TRUE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Test RegisterOnNextSuccessfulWriteReply only.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  file_writer->WriteNow(std::make_unique<std::string>("foo"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_FALSE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(NOT_CALLED,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Test RegisterOnNextWriteSynchronousCallbacks only.
  write_callback_observer_.ObserveNextWriteCallback(pref_store.get());
  file_writer->WriteNow(std::make_unique<std::string>("foo"));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_TRUE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());
}

TEST_F(JsonPrefStoreCallbackTest, TestPostWriteCallbacksWithFakeFailure) {
  scoped_refptr<JsonPrefStore> pref_store = CreatePrefStore();

  // Confirm that the observers are invoked.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  TriggerFakeWriteForCallback(pref_store.get(), true);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Confirm that the observation states were reset.
  EXPECT_FALSE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_EQ(NOT_CALLED,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Confirm that re-installing the observers works for another write.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  TriggerFakeWriteForCallback(pref_store.get(), true);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Confirm that the successful observer is not invoked by an unsuccessful
  // write, and that the synchronous observer is invoked.
  successful_write_reply_observer_.ObserveNextWriteCallback(pref_store.get());
  TriggerFakeWriteForCallback(pref_store.get(), false);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_EQ(CALLED_WITH_ERROR,
            write_callback_observer_.GetAndResetPostWriteObservationState());

  // Do a real write, and confirm that the successful observer was invoked after
  // being set by |PostWriteCallback| by the last TriggerFakeWriteCallback.
  ImportantFileWriter* file_writer = GetImportantFileWriter(pref_store.get());
  file_writer->WriteNow(std::make_unique<std::string>("foo"));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_EQ(NOT_CALLED,
            write_callback_observer_.GetAndResetPostWriteObservationState());
}

TEST_F(JsonPrefStoreCallbackTest, TestPostWriteCallbacksDuringProfileDeath) {
  // Create a JsonPrefStore and attach observers to it, then delete it by making
  // it go out of scope to simulate profile switch or Chrome shutdown.
  {
    scoped_refptr<JsonPrefStore> soon_out_of_scope_pref_store =
        CreatePrefStore();
    ImportantFileWriter* file_writer =
        GetImportantFileWriter(soon_out_of_scope_pref_store.get());
    successful_write_reply_observer_.ObserveNextWriteCallback(
        soon_out_of_scope_pref_store.get());
    write_callback_observer_.ObserveNextWriteCallback(
        soon_out_of_scope_pref_store.get());
    file_writer->WriteNow(std::make_unique<std::string>("foo"));
  }
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(successful_write_reply_observer_.GetAndResetObservationState());
  EXPECT_TRUE(write_callback_observer_.GetAndResetPreWriteObservationState());
  EXPECT_EQ(CALLED_WITH_SUCCESS,
            write_callback_observer_.GetAndResetPostWriteObservationState());
}

}  // namespace base
