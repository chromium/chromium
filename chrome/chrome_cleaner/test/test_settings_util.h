// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_TEST_TEST_SETTINGS_UTIL_H_
#define CHROME_CHROME_CLEANER_TEST_TEST_SETTINGS_UTIL_H_

#include <vector>

#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

// Dummy Settings instance where the value to be returned by execution_mode()
// is defined during object construction. For use with
// |Settings::SetInstanceForTesting|.
class SettingsWithExecutionModeOverride : public Settings {
 public:
  explicit SettingsWithExecutionModeOverride(ExecutionMode execution_mode);
  ~SettingsWithExecutionModeOverride() override;

  ExecutionMode execution_mode() const override;

 private:
  ExecutionMode execution_mode_;
};

class MockSettings : public Settings {
 public:
  MockSettings();
  ~MockSettings() override;

  MOCK_CONST_METHOD0(allow_crash_report_upload, bool());
  MOCK_CONST_METHOD0(session_id, base::string16());
  MOCK_CONST_METHOD0(cleanup_id, std::string());
  MOCK_CONST_METHOD0(engine, Engine::Name());
  MOCK_CONST_METHOD0(is_stub_engine, bool());
  MOCK_CONST_METHOD0(logs_upload_allowed, bool());
  MOCK_CONST_METHOD0(logs_collection_enabled, bool());
  MOCK_CONST_METHOD0(logs_allowed_in_cleanup_mode, bool());
  MOCK_METHOD1(set_logs_allowed_in_cleanup_mode, void(bool));
  MOCK_CONST_METHOD0(metrics_enabled, bool());
  MOCK_CONST_METHOD0(sber_enabled, bool());
  MOCK_CONST_METHOD0(chrome_mojo_pipe_token, const std::string&());
  MOCK_CONST_METHOD0(has_parent_pipe_handle, bool());
  MOCK_CONST_METHOD0(prompt_using_mojo, bool());
  MOCK_CONST_METHOD0(prompt_response_read_handle, HANDLE());
  MOCK_CONST_METHOD0(prompt_request_write_handle, HANDLE());
  MOCK_CONST_METHOD0(switches_valid_for_ipc, bool());
  MOCK_CONST_METHOD0(has_any_ipc_switch, bool());
  MOCK_CONST_METHOD0(execution_mode, ExecutionMode());
  MOCK_CONST_METHOD0(locations_to_scan,
                     const std::vector<UwS::TraceLocation>&());
  MOCK_CONST_METHOD0(open_file_size_limit, int64_t());
  MOCK_CONST_METHOD0(run_without_sandbox_for_testing, bool());
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_TEST_TEST_SETTINGS_UTIL_H_
