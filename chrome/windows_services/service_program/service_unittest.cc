// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <shlobj.h>
#include <wrl/client.h>

#include <map>
#include <memory>
#include <numeric>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner_thread_mode.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/windows_handle_util.h"
#include "chrome/common/env_vars.h"
#include "chrome/windows_services/service_program/test_service_idl.h"
#include "chrome/windows_services/service_program/test_support/service_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"
#include "third_party/crashpad/crashpad/client/prune_crash_reports.h"

namespace {

// Collects log messages from test service processes on behalf of a test.
class LogCollector {
 public:
  explicit LogCollector(ServiceEnvironment& service_environment);
  LogCollector(const LogCollector&) = delete;
  LogCollector& operator=(const LogCollector&) = delete;
  ~LogCollector();

  // Emits all messages that have been collected to the test process's log
  // output in a single message.
  void EmitLogs();

 private:
  // A mapping from a test service's process id to a sequence of intercepted log
  // messages ordered by time of arrival. Due to aggressive PID reuse on
  // Windows, it is possible that messages from distinct instances will be
  // grouped under the same PID.
  using ServiceLogsMap = std::map<base::ProcessId, std::vector<std::string>>;

  // A ServiceEnvironment::LogMessageCallback.
  bool OnLogMessage(base::ProcessId process_id, std::string_view message);

  const raw_ref<ServiceEnvironment> service_environment_;
  base::Lock lock_;
  ServiceLogsMap service_logs_ GUARDED_BY(lock_);
};

LogCollector::LogCollector(ServiceEnvironment& service_environment)
    : service_environment_(service_environment) {
  // Unretained is safe here because the callback is cleared in the dtor.
  service_environment.SetLogMessageCallback(
      base::BindRepeating(&LogCollector::OnLogMessage, base::Unretained(this)));
}

LogCollector::~LogCollector() {
  service_environment_->SetLogMessageCallback({});
}

void LogCollector::EmitLogs() {
  ServiceLogsMap service_logs;
  {
    base::AutoLock lock(lock_);
    service_logs.swap(service_logs_);
  }
  for (const auto& [pid, messages] : service_logs) {
    LOG(ERROR) << "LOG MESSAGES FROM SERVICE PID " << pid << " "
               << testing::PrintToString(messages);
  }
}

bool LogCollector::OnLogMessage(base::ProcessId process_id,
                                std::string_view message) {
  base::AutoLock lock(lock_);
  service_logs_[process_id].emplace_back(message);
  return true;  // Suppress emission of the message by the LogGrabber.
}

}  // namespace

// A test harness that installs the test service at test suite setup time (i.e.,
// once for all tests that use this harness) and provides facilities for calling
// into the service.
class ServiceTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (!::IsUserAnAdmin()) {
      GTEST_SKIP() << "Test requires admin rights";
    }
    service_environment_ = new ServiceEnvironment(
        L"Chromium Test Service", FILE_PATH_LITERAL("test_service.exe"),
        /*testing_switch=*/{}, __uuidof(TestService), __uuidof(ITestService));
    ASSERT_TRUE(service_environment_->is_valid());
  }

  static void TearDownTestSuite() {
    delete std::exchange(service_environment_, nullptr);
  }

  ServiceTest() = default;
  ~ServiceTest() override {
    if (HasFailure()) {  // Emit server logs in case of failure.
      log_collector_.EmitLogs();
    }
  }

  void SetUp() override { ASSERT_TRUE(com_initializer_.Succeeded()); }

  // Instantiates the test service, returning a reference to it in
  // `test_service`. Asserts in case of failure.
  static void CreateService(
      Microsoft::WRL::ComPtr<ITestService>& test_service) {
    Microsoft::WRL::ComPtr<IUnknown> unknown;
    ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(__uuidof(TestService), nullptr,
                                                CLSCTX_LOCAL_SERVER,
                                                IID_PPV_ARGS(&unknown)));

    ASSERT_HRESULT_SUCCEEDED(unknown.As(&test_service));
    unknown.Reset();

    ASSERT_HRESULT_SUCCEEDED(::CoSetProxyBlanket(
        test_service.Get(), RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT,
        COLE_DEFAULT_PRINCIPAL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_DYNAMIC_CLOAKING));
  }

  // Returns a handle to `test_service`'s process in `process`. Asserts in case
  // of failure.
  static void GetServiceProcess(
      Microsoft::WRL::ComPtr<ITestService>& test_service,
      base::Process& process) {
    unsigned long handle_value = 0;
    ASSERT_HRESULT_SUCCEEDED(test_service->GetProcessHandle(&handle_value));
    HANDLE service_process_handle = base::win::Uint32ToHandle(handle_value);
    ASSERT_NE(service_process_handle, nullptr);
    ASSERT_NE(service_process_handle, INVALID_HANDLE_VALUE);
    process = base::Process(service_process_handle);
  }

 private:
  static ServiceEnvironment* service_environment_;

  // The main thread is an STA thread, so it must run a UI message loop.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::win::ScopedCOMInitializer com_initializer_;
  LogCollector log_collector_{*service_environment_};
};

// static
ServiceEnvironment* ServiceTest::service_environment_ = nullptr;

namespace {

// Map a (creation time, pid) pair, identifying a single instance of the test
// service, to a sequence of (tick count, tid) pairs, identifying the client
// thread and time of each request to the service.
using ServiceMap = std::map<std::pair<base::Time, base::ProcessId>,
                            std::vector<std::pair<DWORD, DWORD>>>;

}  // namespace

namespace std {

// Teach Google Test how to print a ServiceMap.
void PrintTo(const ServiceMap& service_map, std::ostream* os) {
  *os << "(";
  bool first = true;
  for (const auto& [key, transactions] : service_map) {
    const auto& [creation_time, pid] = key;
    if (!first) {
      *os << ",";
    } else {
      first = false;
    }
    *os << "(" << creation_time << "," << pid
        << "):" << testing::PrintToString(transactions);
  }
  *os << ")";
}

}  // namespace std

// Tests that a service can handle two requests on the same object.
TEST_F(ServiceTest, TwoRequests) {
  base::Process service_process;
  base::Process service_process2;

  Microsoft::WRL::ComPtr<ITestService> test_service;
  ASSERT_NO_FATAL_FAILURE(CreateService(test_service));
  ASSERT_NO_FATAL_FAILURE(GetServiceProcess(test_service, service_process));
  ASSERT_NO_FATAL_FAILURE(GetServiceProcess(test_service, service_process2));
  test_service.Reset();

  ASSERT_EQ(service_process.Pid(), service_process2.Pid());
  service_process2.Close();
  int exit_code = 0;
  service_process.WaitForExit(&exit_code);
  ASSERT_EQ(exit_code, 0);
}

TEST_F(ServiceTest, IsRunningUnattended) {
  Microsoft::WRL::ComPtr<ITestService> test_service;
  ASSERT_NO_FATAL_FAILURE(CreateService(test_service));
  VARIANT_BOOL is_running_unattended = VARIANT_FALSE;
  ASSERT_HRESULT_SUCCEEDED(
      test_service->IsRunningUnattended(&is_running_unattended));
  ASSERT_EQ(is_running_unattended != VARIANT_FALSE,
            base::Environment::Create()->HasVar(env_vars::kHeadless));
}

// Tests that a service can handle rapid use that should result in some requests
// happening in the same instance of the service as a previous request, while
// some are handled in a separate instance of the service. This is a regression
// test against https://crbug.com/375097840.
TEST_F(ServiceTest, RapidReuse) {
  // Calculate the average time to start the service, get an answer from it, and
  // for it to terminate over five runs. Ignore the first run, as it is expected
  // to be slower.
  base::TimeDelta average_call_time;
  {
    constexpr int kTimingTries = 5;
    for (int i = 0; i < kTimingTries + 1; ++i) {
      base::Process service_process;

      base::ElapsedTimer elapsed_timer;
      Microsoft::WRL::ComPtr<ITestService> test_service;
      ASSERT_NO_FATAL_FAILURE(CreateService(test_service));
      ASSERT_NO_FATAL_FAILURE(GetServiceProcess(test_service, service_process));
      test_service.Reset();
      int exit_code = 0;
      ASSERT_TRUE(service_process.WaitForExit(&exit_code));
      if (i) {  // Ignore the first run.
        average_call_time = elapsed_timer.Elapsed();
      }
      ASSERT_EQ(exit_code, 0);
    }
    average_call_time /= kTimingTries;
  }

  // Now throw off a group of tasks that will race each other to repeatedly call
  // the service at random intervals around `average_call_time`.
  size_t succeeded_count = 0;  // The number of tasks that ran to completion.
  ServiceMap transactions;     // The requests processed by each service.

  // Adds a single task's success/failure and requests collection to the overall
  // stats, then runs a given `quit_closure`. This is run on the main thread
  // following each task's completion.
  auto result_accumulator = base::BindLambdaForTesting(
      [&succeeded_count, &transactions](base::RepeatingClosure quit_closure,
                                        bool succeeded,
                                        const ServiceMap& task_transactions) {
        if (succeeded) {
          ++succeeded_count;
        }
        // Merge this task's requests in with those from all other tasks that
        // have completed so far, sorting each process's by the time at which
        // the client made the request.
        for (const auto& [process, xactions] : task_transactions) {
          auto& combined = transactions[process];
          combined.insert(combined.end(), xactions.begin(), xactions.end());
          std::ranges::stable_sort(
              combined, [](auto& a, auto& b) { return a.first < b.first; });
        }
        std::move(quit_closure).Run();
      });

  // Issues requests to the service in a loop for five seconds; running
  // `on_result` with the results on completion. Asserts in case of failure.
  auto task = base::BindRepeating(
      [](base::TimeDelta average_call_time,
         base::OnceCallback<void(bool, const ServiceMap&)> on_result) {
        bool succeeded = false;
        ServiceMap task_transactions;
        absl::Cleanup return_results = [&on_result, &succeeded,
                                        &task_transactions] {
          std::move(on_result).Run(succeeded, task_transactions);
        };

        base::ProcessId last_pid = base::kNullProcessId;
        const DWORD tid = ::GetCurrentThreadId();
        for (base::ElapsedTimer timer; timer.Elapsed() < base::Seconds(5);) {
          base::Process service_process;
          Microsoft::WRL::ComPtr<ITestService> test_service;
          base::ElapsedTimer transaction_timer;
          const auto tick_count = ::GetTickCount();
          ASSERT_NO_FATAL_FAILURE(CreateService(test_service))
              << "Last service pid: " << last_pid
              << " at tick_count: " << tick_count << " from thread: " << tid;
          ASSERT_NO_FATAL_FAILURE(
              GetServiceProcess(test_service, service_process));
          // Drop the connection.
          test_service.Reset();
          base::ProcessId pid = service_process.Pid();
          ASSERT_NE(pid, base::kNullProcessId);
          base::Time creation_time = service_process.CreationTime();
          ASSERT_FALSE(creation_time.is_null());
          service_process.Close();
          task_transactions[std::make_pair(creation_time, pid)].emplace_back(
              tick_count, tid);
          last_pid = pid;

          // Wait a bit to give the service a chance to get closer to shutting
          // down.
          base::TimeDelta remaining =
              average_call_time - transaction_timer.Elapsed();
          if (remaining > base::TimeDelta()) {
            base::PlatformThread::Sleep(base::RandTimeDeltaUpTo(remaining));
          }
        }
        // If execution reached this point, all requests were handled.
        succeeded = true;
      });

  base::RunLoop run_loop;

  // Reduce to only one thread due to flaky CO_E_SERVER_EXEC_FAILURE; see
  // https://crbug.com/375097840.
  static constexpr int kTaskCount = 1;

  // Quit `run_loop` after all `kTaskCount` tasks have run this closure.
  base::RepeatingClosure quit_barrier =
      base::BarrierClosure(kTaskCount, run_loop.QuitClosure());

  // Issue the tasks.
  for (int i = 0; i < kTaskCount; ++i) {
    base::ThreadPool::CreateCOMSTATaskRunner(
        {}, base::SingleThreadTaskRunnerThreadMode::DEDICATED)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(task, average_call_time,
                           base::BindPostTaskToCurrentDefault(base::BindOnce(
                               result_accumulator, quit_barrier))));
  }
  run_loop.Run();  // Wait for all tasks to return their results.

  if (HasFailure()) {
    LOG(ERROR) << succeeded_count << " out of " << kTaskCount
               << " tasks ran to completion";
    LOG(ERROR) << transactions.size() << " services handled a total of "
               << std::accumulate(
                      transactions.begin(), transactions.end(), size_t(0),
                      [](size_t acc, const ServiceMap::value_type& v) {
                        return acc + v.second.size();
                      })
               << " requests";
    LOG(ERROR) << "transactions: " << testing::PrintToString(transactions);
  }
}

// Crashpad delegates to ASAN's exception handler when `is_asan = true`, so no
// dumps are generated in the crashpad database.
#if !defined(ADDRESS_SANITIZER)

// A test fixture for validating crashpad integration.
class ServiceCrashTest : public ServiceTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(io_thread_.StartWithOptions({base::MessagePumpType::IO,
                                             /*size=*/0}));
  }

  void TearDown() override {
    database_watcher_.SynchronouslyResetForTest();
    DeleteCrashDatabase();
  }

  void CreateServiceForCrashTest(
      Microsoft::WRL::ComPtr<ITestService>& test_service) {
    CreateService(test_service);
    if (!test_service) {
      return;
    }
    if (crashpad_database_path_.empty()) {
      // This is the first connection to the service. Get the path to its
      // crashpad database from the service.
      base::win::ScopedBstr database_path;
      ASSERT_HRESULT_SUCCEEDED(
          test_service->GetCrashpadDatabasePath(database_path.Receive()));
      crashpad_database_path_ = base::FilePath(
          std::wstring_view(database_path.Get(), database_path.Length()));

      // Delete all existing crash reports that may be left over from past
      // executions.
      PruneOldReports();

      // Start watching the crash database for modifications.
      WatchForNewReports();
    }
  }

  // Waits for a new crash report to appear in the service's crash database.
  void WaitForDump() {
    base::RunLoop run_loop;
    on_dump_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  class PruneAllCondition : public crashpad::PruneCondition {
   public:
    // crashpad::PruneCondition:
    bool ShouldPruneReport(
        const crashpad::CrashReportDatabase::Report&) override {
      return true;
    }
    void ResetPruneConditionState() override {}
  };

  // Deletes all existing crash reports from the test service.
  void PruneOldReports() {
    if (auto database =
            crashpad::CrashReportDatabase::InitializeWithoutCreating(
                crashpad_database_path_);
        database) {
      PruneAllCondition all_condition;
      crashpad::PruneCrashReportDatabase(database.get(), &all_condition);

      // Make sure that the database is now empty of all reports.
      std::vector<crashpad::CrashReportDatabase::Report> reports;
      ASSERT_EQ(database->GetPendingReports(&reports),
                crashpad::CrashReportDatabase::kNoError);
      ASSERT_TRUE(reports.empty());
      ASSERT_EQ(database->GetCompletedReports(&reports),
                crashpad::CrashReportDatabase::kNoError);
      ASSERT_TRUE(reports.empty());
    }
  }

  // Starts monitoring for new crash reports to appear in the database.
  void WatchForNewReports() {
    base::RunLoop run_loop;
    database_watcher_.emplace(io_thread_.task_runner());

    // OnDatabaseChange will be called on the main test thread for any change to
    // the crash database's "metadata" file. This file is written to after a
    // dump is written to disk.
    database_watcher_.AsyncCall(&base::FilePathWatcher::WatchWithChangeInfo)
        .WithArgs(
            crashpad_database_path_.Append(FILE_PATH_LITERAL("metadata")),
            base::FilePathWatcher::WatchOptions{
                .type = base::FilePathWatcher::Type::kNonRecursive},
            base::BindPostTaskToCurrentDefault(base::BindRepeating(
                &ServiceCrashTest::OnDatabaseChange, base::Unretained(this))))
        .Then(base::BindOnce(
            [](base::OnceClosure quit_loop, bool succeeded) {
              std::move(quit_loop).Run();
              ASSERT_TRUE(succeeded);
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Deletes the entire crash database from the test service.
  void DeleteCrashDatabase() {
    base::DeletePathRecursively(crashpad_database_path_);
  }

  // Processes a change to the crash database on the main thread.
  void OnDatabaseChange(const base::FilePathWatcher::ChangeInfo& change_info,
                        const base::FilePath& path,
                        bool error) {
    ASSERT_FALSE(error);
    if (!on_dump_closure_) {
      return;  // Not presently waiting for a dump to appear.
    }
    std::optional<crashpad::CrashReportDatabase::Report> report;
    if (auto database =
            crashpad::CrashReportDatabase::InitializeWithoutCreating(
                crashpad_database_path_);
        database) {
      // Search the database for any report. The database is cleared during
      // startup, so any report must be generated by the service under test.
      std::vector<crashpad::CrashReportDatabase::Report> reports;
      ASSERT_EQ(database->GetPendingReports(&reports),
                crashpad::CrashReportDatabase::kNoError);
      if (reports.empty()) {
        ASSERT_EQ(database->GetCompletedReports(&reports),
                  crashpad::CrashReportDatabase::kNoError);
      }
      if (!reports.empty()) {
        report.emplace(std::move(reports.front()));
      }
    }
    if (report.has_value()) {
      std::move(on_dump_closure_).Run();
    }
  }

  // The path to the service's crashpad database.
  base::FilePath crashpad_database_path_;

  // A thread with an IO message loop for using a FilePathWatcher.
  base::Thread io_thread_{"IO Thread"};

  // A watcher to be notified when the service's crashpad database is modified.
  base::SequenceBound<base::FilePathWatcher> database_watcher_;

  // A callback that is run when a dump has been added to the service's crashpad
  // database.
  base::OnceClosure on_dump_closure_;
};

// Tests that a dump is produced if a crash happens during a COM call.
TEST_F(ServiceCrashTest, InduceCrash) {
  Microsoft::WRL::ComPtr<ITestService> test_service;
  ASSERT_NO_FATAL_FAILURE(CreateServiceForCrashTest(test_service));
  ASSERT_EQ(test_service->InduceCrash(), HRESULT_FROM_WIN32(RPC_S_CALL_FAILED));
  WaitForDump();
}

// Tests that a dump is produced if a crash happens in the background.
TEST_F(ServiceCrashTest, InduceCrashSoon) {
  Microsoft::WRL::ComPtr<ITestService> test_service;
  ASSERT_NO_FATAL_FAILURE(CreateServiceForCrashTest(test_service));
  ASSERT_HRESULT_SUCCEEDED(test_service->InduceCrashSoon());
  WaitForDump();
}

#endif  // !defined(ADDRESS_SANITIZER)
