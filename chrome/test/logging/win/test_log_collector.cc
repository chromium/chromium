// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/logging/win/test_log_collector.h"

#include <windows.h>

#include <algorithm>
#include <ios>
#include <memory>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/logging/win/file_logger.h"
#include "chrome/test/logging/win/log_file_printer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging_win {

namespace {

const char kTraceLogExtension[] = ".etl";

class TestLogCollector {
 public:
  TestLogCollector();
  ~TestLogCollector();

  void Initialize(testing::UnitTest* unit_test);

  void SetUp();
  void StartSessionForTest(const testing::TestInfo& test_info);
  bool LogTestPartResult(const testing::TestPartResult& test_part_result);
  void ProcessSessionForTest(const testing::TestInfo& test_info);
  void TearDown();

 private:
  // An EventListener that generally delegates to a given default result
  // printer with a few exceptions; see individual method comments for details.
  class EventListener : public testing::TestEventListener {
   public:
    // Ownership of |default_result_printer| is taken by the new instance.
    EventListener(TestLogCollector* test_log_collector,
                  testing::TestEventListener* default_result_printer);
    ~EventListener() override;

    // Sets up the log collector.
    void OnTestProgramStart(const testing::UnitTest& unit_test) override {
      test_log_collector_->SetUp();
      default_result_printer_->OnTestProgramStart(unit_test);
    }

    void OnTestIterationStart(const testing::UnitTest& unit_test,
                              int iteration) override {
      default_result_printer_->OnTestIterationStart(unit_test, iteration);
    }

    void OnEnvironmentsSetUpStart(const testing::UnitTest& unit_test) override {
      default_result_printer_->OnEnvironmentsSetUpStart(unit_test);
    }

    void OnEnvironmentsSetUpEnd(const testing::UnitTest& unit_test) override {
      default_result_printer_->OnEnvironmentsSetUpEnd(unit_test);
    }

    void OnTestCaseStart(const testing::TestCase& test_case) override {
      default_result_printer_->OnTestCaseStart(test_case);
    }

    // Calls back to the collector to start collecting logs for this test.
    void OnTestStart(const testing::TestInfo& test_info) override {
      default_result_printer_->OnTestStart(test_info);
      test_log_collector_->StartSessionForTest(test_info);
    }

    // Calls back to the collector with the partial result.  If the collector
    // does not handle it, it is given to the default result printer.
    void OnTestPartResult(
        const testing::TestPartResult& test_part_result) override {
      if (!test_log_collector_->LogTestPartResult(test_part_result))
        default_result_printer_->OnTestPartResult(test_part_result);
    }

    // Calls back to the collector to handle the collected log for the test that
    // has just ended.
    void OnTestEnd(const testing::TestInfo& test_info) override {
      test_log_collector_->ProcessSessionForTest(test_info);
      default_result_printer_->OnTestEnd(test_info);
    }

    void OnTestCaseEnd(const testing::TestCase& test_case) override {
      default_result_printer_->OnTestCaseEnd(test_case);
    }

    void OnEnvironmentsTearDownStart(
        const testing::UnitTest& unit_test) override {
      default_result_printer_->OnEnvironmentsTearDownStart(unit_test);
    }

    void OnEnvironmentsTearDownEnd(
        const testing::UnitTest& unit_test) override {
      default_result_printer_->OnEnvironmentsTearDownEnd(unit_test);
    }

    void OnTestIterationEnd(const testing::UnitTest& unit_test,
                            int iteration) override {
      default_result_printer_->OnTestIterationEnd(unit_test, iteration);
    }

    // Tears down the log collector.
    void OnTestProgramEnd(const testing::UnitTest& unit_test) override {
      default_result_printer_->OnTestProgramEnd(unit_test);
      test_log_collector_->TearDown();
    }

   private:
    TestLogCollector* test_log_collector_;
    std::unique_ptr<testing::TestEventListener> default_result_printer_;

    DISALLOW_COPY_AND_ASSIGN(EventListener);
  };

  // The Google Test unit test into which the collector has been installed.
  testing::UnitTest* unit_test_;

  // A temporary directory into which a log file is placed for the duration of
  // each test.  Created/destroyed at collector SetUp and TearDown.
  base::ScopedTempDir log_temp_dir_;

  // The test logger.  Initialized/Unintitialized at collector SetUp and
  // TearDown.
  std::unique_ptr<FileLogger> file_logger_;

  // The current log file.  Valid only during a test.
  base::FilePath log_file_;

  // True if --also-emit-success-logs was specified on the command line.
  bool also_emit_success_logs_;

  DISALLOW_COPY_AND_ASSIGN(TestLogCollector);
};

base::LazyInstance<TestLogCollector>::DestructorAtExit g_test_log_collector =
    LAZY_INSTANCE_INITIALIZER;

// TestLogCollector::EventListener implementation

TestLogCollector::EventListener::EventListener(
    TestLogCollector* test_log_collector,
    testing::TestEventListener* default_result_printer)
    : test_log_collector_(test_log_collector),
      default_result_printer_(default_result_printer) {
}

TestLogCollector::EventListener::~EventListener() {
}

// TestLogCollector implementation

TestLogCollector::TestLogCollector()
    : unit_test_(nullptr), also_emit_success_logs_(false) {}

TestLogCollector::~TestLogCollector() {
}

void TestLogCollector::Initialize(testing::UnitTest* unit_test) {
  if (unit_test_ != nullptr) {
    CHECK_EQ(unit_test, unit_test_)
        << "Cannot install the test log collector in multiple unit tests.";
    return;  // Already initialized.
  }

  // Remove the default result printer and install the collector's listener
  // which delegates to the printer.  If the default result printer has already
  // been released, log an error and move on.
  testing::TestEventListeners& listeners = unit_test->listeners();
  testing::TestEventListener* default_result_printer =
      listeners.default_result_printer();
  if (default_result_printer == NULL) {
    LOG(ERROR) << "Failed to initialize the test log collector on account of "
                  "another component having released the default result "
                  "printer.";
  } else {
    // Ownership of |default_release_printer| is passed to the new listener, and
    // ownership of the new listener is passed to the unit test.
    listeners.Append(
        new EventListener(this, listeners.Release(default_result_printer)));

    also_emit_success_logs_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kAlsoEmitSuccessLogs);

    unit_test_ = unit_test;
  }
}

// Invoked by the listener at test program start to create the temporary log
// directory and initialize the logger.
void TestLogCollector::SetUp() {
  if (!log_temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Failed to create temporary directory to hold log files.";
  } else {
    file_logger_.reset(new FileLogger());
    file_logger_->Initialize();
  }
}

// Invoked by the listener at test start to begin collecting logs in a file.
void TestLogCollector::StartSessionForTest(const testing::TestInfo& test_info) {
  if (log_temp_dir_.IsValid()) {
    std::string log_file_name(test_info.name());
    std::replace(log_file_name.begin(), log_file_name.end(), '/', '_');
    log_file_name.append(kTraceLogExtension);
    log_file_ = log_temp_dir_.GetPath().AppendASCII(log_file_name);

    file_logger_->StartLogging(log_file_);
  }
}

// Invoked by the listener when a test result is produced to log an event for
// the result.
bool TestLogCollector::LogTestPartResult(
    const testing::TestPartResult& test_part_result) {
  // Can't handle the event if no trace session.
  if (!file_logger_.get() || !file_logger_->is_logging())
    return false;

  if (test_part_result.type() != testing::TestPartResult::kSuccess) {
    // Approximate Google Test's message formatting.
    LOG(ERROR)
        << base::StringPrintf("%s(%d): error: %s", test_part_result.file_name(),
                              test_part_result.line_number(),
                              test_part_result.message());
  }
  return true;
}

// Invoked by the listener at test end to dump the collected log in case of
// error.
void TestLogCollector::ProcessSessionForTest(
    const testing::TestInfo& test_info) {
  if (file_logger_.get() != NULL && file_logger_->is_logging()) {
    file_logger_->StopLogging();

    if (also_emit_success_logs_ || test_info.result()->Failed()) {
      std::cerr << "----- log messages for "
                << test_info.test_case_name() << "." << test_info.name()
                << " above this line are repeated below -----" << std::endl;
      // Dump the log to stderr.
      logging_win::PrintLogFile(log_file_, &std::cerr);
      std::cerr.flush();
    }

    if (!base::DeleteFile(log_file_))
      LOG(ERROR) << "Failed to delete log file " << log_file_.value();
  }

  log_file_.clear();
}

// Invoked by the listener at test program end to shut down the logger and
// delete the temporary log directory.
void TestLogCollector::TearDown() {
  file_logger_.reset();

  ignore_result(log_temp_dir_.Delete());
}

}  // namespace

void InstallTestLogCollector(testing::UnitTest* unit_test) {
  // Must be called before running any tests.
  DCHECK(unit_test);
  DCHECK(!unit_test->current_test_case());

  g_test_log_collector.Get().Initialize(unit_test);
}

}  // namespace logging_win
