// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/logging/win/file_logger.h"

#include <limits.h>
#include <windows.h>
#include <guiddef.h>
#include <objbase.h>
#include <stddef.h>

#include <ios>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/event_trace_consumer.h"
#include "base/win/registry.h"

namespace logging_win {

namespace {

const wchar_t kChromeTestSession[] = L"chrome_tests";

// From chrome_tab.cc: {0562BFC3-2550-45b4-BD8E-A310583D3A6F}
const GUID kChromeFrameProvider =
    { 0x562bfc3, 0x2550, 0x45b4,
        { 0xbd, 0x8e, 0xa3, 0x10, 0x58, 0x3d, 0x3a, 0x6f } };

// From chrome/common/logging_chrome.cc: {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName =
    { 0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };

// {81729947-CD2A-49e6-8885-785429F339F5}
const GUID kChromeTestsProvider =
    { 0x81729947, 0xcd2a, 0x49e6,
        { 0x88, 0x85, 0x78, 0x54, 0x29, 0xf3, 0x39, 0xf5 } };

// The configurations for the supported providers.  This must be in sync with
// FileLogger::EventProviderBits.
const struct {
  const GUID* provider_name;
  uint8_t level;
  uint32_t flags;
} kProviders[] = {
  { &kChromeTraceProviderName, 255, 0 },
  { &kChromeFrameProvider, 255, 0 },
  { &kChromeTestsProvider, 255, 0 },
};

static_assert((1 << base::size(kProviders)) - 1 ==
                  FileLogger::kAllEventProviders,
              "size of kProviders is inconsistent with kAllEventProviders");

}  // namespace

bool FileLogger::is_initialized_ = false;

FileLogger::FileLogger()
    : event_provider_mask_() {
}

FileLogger::~FileLogger() {
  if (is_logging()) {
    LOG(ERROR)
      << __FUNCTION__ << " don't forget to call FileLogger::StopLogging()";
    StopLogging();
  }

  is_initialized_ = false;
}

// Returns false if all providers could not be enabled.  A log message is
// produced for each provider that could not be enabled.
bool FileLogger::EnableProviders() {
  // Default to false if there's at least one provider.
  bool result = (event_provider_mask_ == 0);

  // Generate ETW log events for this test binary.  Log messages at and above
  // logging::GetMinLogLevel() will continue to go to stderr as well.  This
  // leads to double logging in case of test failures: each LOG statement at
  // or above the min level will go to stderr during test execution, and then
  // all events logged to the file session will be dumped again.  If this
  // turns out to be an issue, one could call logging::SetMinLogLevel(INT_MAX)
  // here (stashing away the previous min log level to be restored in
  // DisableProviders) to suppress stderr logging during test execution.  Then
  // those events in the file that were logged at/above the old min level from
  // the test binary could be dumped to stderr if there were no failures.
  if (event_provider_mask_ & CHROME_TESTS_LOG_PROVIDER)
    logging::LogEventProvider::Initialize(kChromeTestsProvider);

  HRESULT hr = S_OK;
  for (size_t i = 0; i < base::size(kProviders); ++i) {
    if (event_provider_mask_ & (1 << i)) {
      hr = controller_.EnableProvider(*kProviders[i].provider_name,
                                      kProviders[i].level,
                                      kProviders[i].flags);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to enable event provider " << i
                   << "; hr=" << std::hex << hr;
      } else {
        result = true;
      }
    }
  }

  return result;
}

void FileLogger::DisableProviders() {
  HRESULT hr = S_OK;
  for (size_t i = 0; i < base::size(kProviders); ++i) {
    if (event_provider_mask_ & (1 << i)) {
      hr = controller_.DisableProvider(*kProviders[i].provider_name);
      LOG_IF(ERROR, FAILED(hr)) << "Failed to disable event provider "
                                << i << "; hr=" << std::hex << hr;
    }
  }

  if (event_provider_mask_ & CHROME_TESTS_LOG_PROVIDER)
    logging::LogEventProvider::Uninitialize();
}

void FileLogger::Initialize() {
  Initialize(kAllEventProviders);
}

void FileLogger::Initialize(uint32_t event_provider_mask) {
  CHECK(!is_initialized_);

  // Stop a previous session that wasn't shut down properly.
  base::win::EtwTraceProperties ignore;
  HRESULT hr = base::win::EtwTraceController::Stop(kChromeTestSession,
                                                   &ignore);
  LOG_IF(ERROR, FAILED(hr) &&
             hr != HRESULT_FROM_WIN32(ERROR_WMI_INSTANCE_NOT_FOUND))
      << "Failed to stop a previous trace session; hr=" << std::hex << hr;

  event_provider_mask_ = event_provider_mask;

  is_initialized_ = true;
}

bool FileLogger::StartLogging(const base::FilePath& log_file) {
  HRESULT hr =
      controller_.StartFileSession(kChromeTestSession,
                                   log_file.value().c_str(), false);
  if (SUCCEEDED(hr)) {
    // Ignore the return value here in the hopes that at least one provider was
    // enabled.
    if (!EnableProviders()) {
      LOG(ERROR) << "Failed to enable any provider.";
      controller_.Stop(NULL);
      return false;
    }
  } else {
    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) {
      LOG(WARNING) << "Access denied while trying to start trace session. "
                      "This is expected when not running as an administrator.";
    } else {
      LOG(ERROR) << "Failed to start trace session to file " << log_file.value()
                 << "; hr=" << std::hex << hr;
    }
    return false;
  }
  return true;
}

void FileLogger::StopLogging() {
  HRESULT hr = S_OK;

  DisableProviders();

  hr = controller_.Flush(NULL);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to flush events; hr=" << std::hex << hr;
  hr = controller_.Stop(NULL);
  LOG_IF(ERROR, FAILED(hr))
      << "Failed to stop ETW session; hr=" << std::hex << hr;
}

}  // namespace logging_win
