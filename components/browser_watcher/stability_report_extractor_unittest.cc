// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_report_extractor.h"

#include <memory>
#include <utility>

#include "base/debug/activity_tracker.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace browser_watcher {

using base::debug::ActivityData;
using base::debug::ActivityTrackerMemoryAllocator;
using base::debug::ActivityUserData;
using base::debug::GlobalActivityTracker;
using base::debug::ThreadActivityTracker;
using base::File;
using base::FilePath;
using base::FilePersistentMemoryAllocator;
using base::MemoryMappedFile;
using base::PersistentMemoryAllocator;

namespace {

// The tracker creates some data entries internally.
const size_t kInternalProcessDatums = 1;

// Parameters for the activity tracking.
const size_t kFileSize = 64 << 10;  // 64 KiB
const int kStackDepth = 6;
const uint64_t kAllocatorId = 0;
const char kAllocatorName[] = "PostmortemReportCollectorCollectionTest";
const uint64_t kTaskSequenceNum = 42;
const uintptr_t kTaskOrigin = 1000U;
const uintptr_t kLockAddress = 1001U;
const uintptr_t kEventAddress = 1002U;
const int kThreadId = 43;
const int kProcessId = 44;
const int kAnotherThreadId = 45;
const uint32_t kGenericId = 46U;
const int32_t kGenericData = 47;

}  // namespace

// Sets up a file backed thread tracker for direct access. A
// GlobalActivityTracker is not created, meaning there is no risk of
// the instrumentation interfering with the file's content.
class StabilityReportExtractorThreadTrackerTest : public testing::Test {
 public:
  // Create a proper debug file.
  void SetUp() override {
    testing::Test::SetUp();

    // Create a file backed allocator.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    debug_file_path_ = temp_dir_.GetPath().AppendASCII("debug_file.pma");
    allocator_ = CreateAllocator();
    ASSERT_NE(nullptr, allocator_);

    size_t tracker_mem_size =
        ThreadActivityTracker::SizeForStackDepth(kStackDepth);
    ASSERT_GT(kFileSize, tracker_mem_size);

    // Create a tracker.
    tracker_ = CreateTracker(allocator_.get(), tracker_mem_size);
    ASSERT_NE(nullptr, tracker_);
    ASSERT_TRUE(tracker_->IsValid());
  }

  std::unique_ptr<PersistentMemoryAllocator> CreateAllocator() {
    // Create the memory mapped file.
    std::unique_ptr<MemoryMappedFile> mmfile(new MemoryMappedFile());
    bool success = mmfile->Initialize(
        File(debug_file_path_, File::FLAG_CREATE | File::FLAG_READ |
                                   File::FLAG_WRITE | File::FLAG_SHARE_DELETE),
        {0, static_cast<int64_t>(kFileSize)},
        MemoryMappedFile::READ_WRITE_EXTEND);
    if (!success || !mmfile->IsValid())
      return nullptr;

    // Create a persistent memory allocator.
    if (!FilePersistentMemoryAllocator::IsFileAcceptable(*mmfile, true))
      return nullptr;
    return std::make_unique<FilePersistentMemoryAllocator>(
        std::move(mmfile), kFileSize, kAllocatorId, kAllocatorName, false);
  }

  std::unique_ptr<ThreadActivityTracker> CreateTracker(
      PersistentMemoryAllocator* allocator,
      size_t tracker_mem_size) {
    // Allocate a block of memory for the tracker to use.
    PersistentMemoryAllocator::Reference mem_reference = allocator->Allocate(
        tracker_mem_size, GlobalActivityTracker::kTypeIdActivityTracker);
    if (mem_reference == 0U)
      return nullptr;

    // Get the memory's base address.
    void* mem_base = allocator->GetAsArray<char>(
        mem_reference, GlobalActivityTracker::kTypeIdActivityTracker,
        PersistentMemoryAllocator::kSizeAny);
    if (mem_base == nullptr)
      return nullptr;

    // Make the allocation iterable so it can be found by other processes.
    allocator->MakeIterable(mem_reference);

    return std::make_unique<ThreadActivityTracker>(mem_base, tracker_mem_size);
  }

  void PerformBasicReportValidation(const StabilityReport& report) {
    // One report with one thread that has the expected name and id.
    ASSERT_EQ(1, report.process_states_size());
    const ProcessState& process_state = report.process_states(0);
    EXPECT_EQ(base::GetCurrentProcId(), process_state.process_id());
    ASSERT_EQ(1, process_state.threads_size());
    const ThreadState& thread_state = process_state.threads(0);
    EXPECT_EQ(base::PlatformThread::GetName(), thread_state.thread_name());
#if defined(OS_WIN)
    EXPECT_EQ(base::PlatformThread::CurrentId(), thread_state.thread_id());
#elif defined(OS_POSIX)
    EXPECT_EQ(base::PlatformThread::CurrentHandle().platform_handle(),
              thread_state.thread_id());
#endif
  }

  const FilePath& debug_file_path() const { return debug_file_path_; }

 protected:
  base::ScopedTempDir temp_dir_;
  FilePath debug_file_path_;

  std::unique_ptr<PersistentMemoryAllocator> allocator_;
  std::unique_ptr<ThreadActivityTracker> tracker_;
};

TEST_F(StabilityReportExtractorThreadTrackerTest, CollectSuccess) {
  // Create some activity data.
  tracker_->PushActivity(reinterpret_cast<void*>(kTaskOrigin),
                         base::debug::Activity::ACT_TASK_RUN,
                         ActivityData::ForTask(kTaskSequenceNum));
  tracker_->PushActivity(
      nullptr, base::debug::Activity::ACT_LOCK_ACQUIRE,
      ActivityData::ForLock(reinterpret_cast<void*>(kLockAddress)));
  ThreadActivityTracker::ActivityId activity_id = tracker_->PushActivity(
      nullptr, base::debug::Activity::ACT_EVENT_WAIT,
      ActivityData::ForEvent(reinterpret_cast<void*>(kEventAddress)));
  tracker_->PushActivity(nullptr, base::debug::Activity::ACT_THREAD_JOIN,
                         ActivityData::ForThread(kThreadId));
  tracker_->PushActivity(nullptr, base::debug::Activity::ACT_PROCESS_WAIT,
                         ActivityData::ForProcess(kProcessId));
  tracker_->PushActivity(nullptr, base::debug::Activity::ACT_GENERIC,
                         ActivityData::ForGeneric(kGenericId, kGenericData));
  // Note: this exceeds the activity stack's capacity.
  tracker_->PushActivity(nullptr, base::debug::Activity::ACT_THREAD_JOIN,
                         ActivityData::ForThread(kAnotherThreadId));

  // Add some user data.
  ActivityTrackerMemoryAllocator user_data_allocator(
      allocator_.get(), GlobalActivityTracker::kTypeIdUserDataRecord,
      GlobalActivityTracker::kTypeIdUserDataRecordFree, 1024U, 10U, false);
  std::unique_ptr<ActivityUserData> user_data =
      tracker_->GetUserData(activity_id, &user_data_allocator);
  user_data->SetInt("some_int", 42);

  // Validate collection returns the expected report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));

  // Validate the report.
  ASSERT_NO_FATAL_FAILURE(PerformBasicReportValidation(report));
  const ThreadState& thread_state = report.process_states(0).threads(0);

  EXPECT_EQ(7, thread_state.activity_count());
  ASSERT_EQ(6, thread_state.activities_size());
  {
    const Activity& activity = thread_state.activities(0);
    EXPECT_EQ(Activity::ACT_TASK_RUN, activity.type());
    EXPECT_EQ(kTaskOrigin, activity.origin_address());
    EXPECT_EQ(kTaskSequenceNum, activity.task_sequence_id());
    EXPECT_EQ(0U, activity.user_data().size());
  }
  {
    const Activity& activity = thread_state.activities(1);
    EXPECT_EQ(Activity::ACT_LOCK_ACQUIRE, activity.type());
    EXPECT_EQ(kLockAddress, activity.lock_address());
    EXPECT_EQ(0U, activity.user_data().size());
  }
  {
    const Activity& activity = thread_state.activities(2);
    EXPECT_EQ(Activity::ACT_EVENT_WAIT, activity.type());
    EXPECT_EQ(kEventAddress, activity.event_address());
    ASSERT_EQ(1U, activity.user_data().size());
    ASSERT_TRUE(base::Contains(activity.user_data(), "some_int"));
    EXPECT_EQ(TypedValue::kSignedValue,
              activity.user_data().at("some_int").value_case());
    EXPECT_EQ(42, activity.user_data().at("some_int").signed_value());
  }
  {
    const Activity& activity = thread_state.activities(3);
    EXPECT_EQ(Activity::ACT_THREAD_JOIN, activity.type());
    EXPECT_EQ(kThreadId, activity.thread_id());
    EXPECT_EQ(0U, activity.user_data().size());
  }
  {
    const Activity& activity = thread_state.activities(4);
    EXPECT_EQ(Activity::ACT_PROCESS_WAIT, activity.type());
    EXPECT_EQ(kProcessId, activity.process_id());
    EXPECT_EQ(0U, activity.user_data().size());
  }
  {
    const Activity& activity = thread_state.activities(5);
    EXPECT_EQ(Activity::ACT_GENERIC, activity.type());
    EXPECT_EQ(kGenericId, activity.generic_id());
    EXPECT_EQ(kGenericData, activity.generic_data());
    EXPECT_EQ(0U, activity.user_data().size());
  }
}

TEST_F(StabilityReportExtractorThreadTrackerTest, CollectException) {
  const void* expected_pc = reinterpret_cast<void*>(0xCAFE);
  const void* expected_address = nullptr;
  const uint32_t expected_code = 42U;

  // Record an exception.
  const int64_t timestamp = base::Time::Now().ToInternalValue();
  tracker_->RecordExceptionActivity(expected_pc, expected_address,
                                    base::debug::Activity::ACT_EXCEPTION,
                                    ActivityData::ForException(expected_code));

  // Collect report and validate.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));

  // Validate the presence of the exception.
  ASSERT_NO_FATAL_FAILURE(PerformBasicReportValidation(report));
  const ThreadState& thread_state = report.process_states(0).threads(0);
  ASSERT_TRUE(thread_state.has_exception());
  const Exception& exception = thread_state.exception();
  EXPECT_EQ(expected_code, exception.code());
  EXPECT_EQ(expected_pc, reinterpret_cast<void*>(exception.program_counter()));
  EXPECT_EQ(expected_address,
            reinterpret_cast<void*>(exception.exception_address()));
  const int64_t tolerance_us = 1000ULL;
  EXPECT_LE(std::abs(timestamp - exception.time()), tolerance_us);
}

TEST_F(StabilityReportExtractorThreadTrackerTest, CollectNoException) {
  // Record something.
  tracker_->PushActivity(reinterpret_cast<void*>(kTaskOrigin),
                         base::debug::Activity::ACT_TASK_RUN,
                         ActivityData::ForTask(kTaskSequenceNum));

  // Collect report and validate there is no exception.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));
  ASSERT_NO_FATAL_FAILURE(PerformBasicReportValidation(report));
  const ThreadState& thread_state = report.process_states(0).threads(0);
  ASSERT_FALSE(thread_state.has_exception());
}

// Tests stability report extraction.
class StabilityReportExtractorTest : public testing::Test {
 public:
  const int kMemorySize = 1 << 20;  // 1MiB

  StabilityReportExtractorTest() {}
  ~StabilityReportExtractorTest() override {
    GlobalActivityTracker* global_tracker = GlobalActivityTracker::Get();
    if (global_tracker) {
      global_tracker->ReleaseTrackerForCurrentThreadForTesting();
      delete global_tracker;
    }
  }

  void SetUp() override {
    testing::Test::SetUp();

    // Set up a debug file path.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    debug_file_path_ = temp_dir_.GetPath().AppendASCII("debug.pma");
  }

  const FilePath& debug_file_path() { return debug_file_path_; }

 protected:
  base::ScopedTempDir temp_dir_;
  FilePath debug_file_path_;
};

TEST_F(StabilityReportExtractorTest, LogCollection) {
  // Record some log messages.
  GlobalActivityTracker::CreateWithFile(debug_file_path(), kMemorySize, 0ULL,
                                        "", 3);
  GlobalActivityTracker::Get()->RecordLogMessage("hello world");
  GlobalActivityTracker::Get()->RecordLogMessage("foo bar");

  // Collect the stability report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));

  // Validate the report's log content.
  ASSERT_EQ(2, report.log_messages_size());
  ASSERT_EQ("hello world", report.log_messages(0));
  ASSERT_EQ("foo bar", report.log_messages(1));
}

TEST_F(StabilityReportExtractorTest, ProcessUserDataCollection) {
  const char string1[] = "foo";
  const char string2[] = "bar";

  // Record some process user data.
  GlobalActivityTracker::CreateWithFile(debug_file_path(), kMemorySize, 0ULL,
                                        "", 3);
  ActivityUserData& process_data = GlobalActivityTracker::Get()->process_data();
  ActivityUserData::Snapshot snapshot;
  ASSERT_TRUE(process_data.CreateSnapshot(&snapshot));
  ASSERT_EQ(kInternalProcessDatums, snapshot.size());
  process_data.Set("raw", "foo", 3);
  process_data.SetString("string", "bar");
  process_data.SetChar("char", '9');
  process_data.SetInt("int", -9999);
  process_data.SetUint("uint", 9999);
  process_data.SetBool("bool", true);
  process_data.SetReference("ref", string1, strlen(string1));
  process_data.SetStringReference("sref", string2);

  // Collect the stability report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));

  // We expect a single process.
  ASSERT_EQ(1, report.process_states_size());

  // Validate the report contains the process' data.
  const auto& collected_data = report.process_states(0).data();
  ASSERT_EQ(kInternalProcessDatums + 8U, collected_data.size());

  ASSERT_TRUE(base::Contains(collected_data, "raw"));
  EXPECT_EQ(TypedValue::kBytesValue, collected_data.at("raw").value_case());
  EXPECT_EQ("foo", collected_data.at("raw").bytes_value());

  ASSERT_TRUE(base::Contains(collected_data, "string"));
  EXPECT_EQ(TypedValue::kStringValue, collected_data.at("string").value_case());
  EXPECT_EQ("bar", collected_data.at("string").string_value());

  ASSERT_TRUE(base::Contains(collected_data, "char"));
  EXPECT_EQ(TypedValue::kCharValue, collected_data.at("char").value_case());
  EXPECT_EQ("9", collected_data.at("char").char_value());

  ASSERT_TRUE(base::Contains(collected_data, "int"));
  EXPECT_EQ(TypedValue::kSignedValue, collected_data.at("int").value_case());
  EXPECT_EQ(-9999, collected_data.at("int").signed_value());

  ASSERT_TRUE(base::Contains(collected_data, "uint"));
  EXPECT_EQ(TypedValue::kUnsignedValue, collected_data.at("uint").value_case());
  EXPECT_EQ(9999U, collected_data.at("uint").unsigned_value());

  ASSERT_TRUE(base::Contains(collected_data, "bool"));
  EXPECT_EQ(TypedValue::kBoolValue, collected_data.at("bool").value_case());
  EXPECT_TRUE(collected_data.at("bool").bool_value());

  ASSERT_TRUE(base::Contains(collected_data, "ref"));
  EXPECT_EQ(TypedValue::kBytesReference, collected_data.at("ref").value_case());
  const TypedValue::Reference& ref = collected_data.at("ref").bytes_reference();
  EXPECT_EQ(reinterpret_cast<uintptr_t>(string1), ref.address());
  EXPECT_EQ(strlen(string1), static_cast<uint64_t>(ref.size()));

  ASSERT_TRUE(base::Contains(collected_data, "sref"));
  EXPECT_EQ(TypedValue::kStringReference,
            collected_data.at("sref").value_case());
  const TypedValue::Reference& sref =
      collected_data.at("sref").string_reference();
  EXPECT_EQ(reinterpret_cast<uintptr_t>(string2), sref.address());
  EXPECT_EQ(strlen(string2), static_cast<uint64_t>(sref.size()));
}

TEST_F(StabilityReportExtractorTest, FieldTrialCollection) {
  // Record some data.
  GlobalActivityTracker::CreateWithFile(debug_file_path(), kMemorySize, 0ULL,
                                        "", 3);
  ActivityUserData& process_data = GlobalActivityTracker::Get()->process_data();
  process_data.SetString("string", "bar");
  process_data.SetString("FieldTrial.string", "bar");
  process_data.SetString("FieldTrial.foo", "bar");

  // Collect the stability report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));
  ASSERT_EQ(1, report.process_states_size());

  // Validate the report's experiment and global data.
  ASSERT_EQ(2, report.field_trials_size());
  EXPECT_NE(0U, report.field_trials(0).name_id());
  EXPECT_NE(0U, report.field_trials(0).group_id());
  EXPECT_NE(0U, report.field_trials(1).name_id());
  EXPECT_EQ(report.field_trials(0).group_id(),
            report.field_trials(1).group_id());

  // Expect 1 key/value pair.
  const auto& collected_data = report.process_states(0).data();
  EXPECT_EQ(kInternalProcessDatums + 1U, collected_data.size());
  EXPECT_TRUE(base::Contains(collected_data, "string"));
}

TEST_F(StabilityReportExtractorTest, ModuleCollection) {
  // Record some module information.
  GlobalActivityTracker::CreateWithFile(debug_file_path(), kMemorySize, 0ULL,
                                        "", 3);

  base::debug::GlobalActivityTracker::ModuleInfo module_info = {};
  module_info.is_loaded = true;
  module_info.address = 0x123456;
  module_info.load_time = 1111LL;
  module_info.size = 0x2d000;
  module_info.timestamp = 0xCAFECAFE;
  module_info.age = 1;
  crashpad::UUID debug_uuid;
  debug_uuid.InitializeFromString("11223344-5566-7788-abcd-0123456789ab");
  memcpy(module_info.identifier, &debug_uuid, sizeof(module_info.identifier));
  module_info.file = "foo";
  module_info.debug_file = "bar";

  GlobalActivityTracker::Get()->RecordModuleInfo(module_info);

  // Collect the stability report.
  StabilityReport report;
  ASSERT_EQ(SUCCESS, Extract(debug_file_path(), &report));

  // Validate the report's modules content.
  ASSERT_EQ(1, report.process_states_size());
  const ProcessState& process_state = report.process_states(0);
  ASSERT_EQ(1, process_state.modules_size());

  const CodeModule collected_module = process_state.modules(0);
  EXPECT_EQ(module_info.address,
            static_cast<uintptr_t>(collected_module.base_address()));
  EXPECT_EQ(module_info.size, static_cast<size_t>(collected_module.size()));
  EXPECT_EQ(module_info.file, collected_module.code_file());
  EXPECT_EQ("CAFECAFE2d000", collected_module.code_identifier());
  EXPECT_EQ(module_info.debug_file, collected_module.debug_file());
  EXPECT_EQ("1122334455667788ABCD0123456789AB1",
            collected_module.debug_identifier());
  EXPECT_EQ("", collected_module.version());
  EXPECT_EQ(0LL, collected_module.shrink_down_delta());
  EXPECT_EQ(!module_info.is_loaded, collected_module.is_unloaded());
}

}  // namespace browser_watcher
