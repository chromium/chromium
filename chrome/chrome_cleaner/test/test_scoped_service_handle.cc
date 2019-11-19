// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/test/test_scoped_service_handle.h"

#include <windows.h>

#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process_iterator.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/os/system_util_cleaner.h"
#include "chrome/chrome_cleaner/test/test_executables.h"

namespace chrome_cleaner {

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

// The sleep time in ms between each poll attempt to get information about a
// service.
constexpr unsigned int kServiceQueryWaitTimeMs = 250;

// The number of attempts to contact a service.
constexpr int kServiceQueryRetry = 5;

std::string LastErrorString() {
  return logging::SystemErrorCodeToString(logging::GetLastSystemErrorCode());
}

}  // namespace

TestScopedServiceHandle::~TestScopedServiceHandle() {
  StopAndDelete();
}

AssertionResult TestScopedServiceHandle::InstallService() {
  // Construct the full-path of the test service.
  base::FilePath module_path;
  if (!base::PathService::Get(base::DIR_EXE, &module_path)) {
    return AssertionFailure()
           << "Cannot retrieve module name:" << LastErrorString();
  }
  module_path = module_path.Append(kTestServiceExecutableName);

  DCHECK(!service_manager_.IsValid());
  DCHECK(!service_.IsValid());
  DCHECK(service_name_.empty());

  // Find an unused name for this service.
  base::string16 service_name = RandomUnusedServiceNameForTesting();

  // Get a handle to the service manager.
  service_manager_.Set(
      ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS));
  if (!service_manager_.IsValid()) {
    return AssertionFailure()
           << "Cannot open service manager:" << LastErrorString();
  }

  const base::string16 service_desc =
      base::StrCat({service_name, L" - Chrome Cleanup Tool (test)"});

  service_.Set(::CreateServiceW(
      service_manager_.Get(), service_name.c_str(), service_desc.c_str(),
      SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
      SERVICE_ERROR_NORMAL, module_path.value().c_str(), nullptr, nullptr,
      nullptr, nullptr, nullptr));
  if (!service_.IsValid()) {
    // Unable to create the service.
    return AssertionFailure() << "Cannot create service '" << service_name
                              << "':" << LastErrorString();
  }
  LOG(INFO) << "Created test service '" << service_name << "'.";
  service_name_ = service_name;
  return AssertionSuccess();
}

AssertionResult TestScopedServiceHandle::StartService() {
  if (!::StartService(service_.Get(), 0, nullptr)) {
    return AssertionFailure()
           << "Failed to start " << service_name_ << ":" << LastErrorString();
  }
  SERVICE_STATUS service_status = {};
  for (int iteration = 0; iteration < kServiceQueryRetry; ++iteration) {
    if (!::QueryServiceStatus(service_.Get(), &service_status)) {
      return AssertionFailure()
             << "Failed to query service status:" << LastErrorString();
    }
    if (service_status.dwCurrentState == SERVICE_RUNNING)
      return AssertionSuccess();
    ::Sleep(kServiceQueryWaitTimeMs);
  }
  return AssertionFailure() << "Service was not running after polling "
                            << kServiceQueryRetry << " times";
}

AssertionResult TestScopedServiceHandle::StopAndDelete() {
  Close();
  base::string16 service_name;
  std::swap(service_name, service_name_);
  if (service_name.empty()) {
    return AssertionFailure() << "Attempt to stop service with no name";
  }

  if (!StopService(service_name.c_str())) {
    return AssertionFailure() << "Failed to stop service " << service_name;
  }
  if (!DeleteService(service_name.c_str())) {
    return AssertionFailure() << "Failed to delete service " << service_name;
  }
  if (!WaitForServiceDeleted(service_name.c_str())) {
    return AssertionFailure()
           << "Failed while waiting for deletion of service " << service_name;
  }
  return AssertionSuccess();
}

void TestScopedServiceHandle::Close() {
  service_.Close();
  service_manager_.Close();
}

base::string16 RandomUnusedServiceNameForTesting() {
  base::string16 service_name;
  do {
    service_name =
        base::UTF8ToUTF16(base::UnguessableToken::Create().ToString());
  } while (DoesServiceExist(service_name.c_str()));
  return service_name;
}

AssertionResult EnsureNoTestServicesRunning() {
  // Get the pid's of all processes running the test service executable.
  base::ProcessIterator::ProcessEntries processes =
      base::NamedProcessIterator(kTestServiceExecutableName, nullptr)
          .Snapshot();
  if (processes.empty())
    return AssertionSuccess();

  std::vector<base::ProcessId> process_ids;
  process_ids.reserve(processes.size());
  for (const auto& process : processes) {
    process_ids.push_back(process.pid());
  }

  // Iterate through all installed services. Stop and delete all those with
  // pid's in the list.
  ScopedScHandle service_manager(::OpenSCManager(
      nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));

  std::vector<ServiceStatus> services;
  if (!EnumerateServices(service_manager, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_STATE_ALL, &services)) {
    return AssertionFailure() << "Failed to enumerate services";
  }
  std::vector<base::string16> stopped_service_names;
  for (const ServiceStatus& service : services) {
    base::string16 service_name = service.service_name;
    base::ProcessId pid = service.service_status_process.dwProcessId;
    if (base::Contains(process_ids, pid)) {
      if (!StopService(service_name.c_str()))
        return AssertionFailure() << "Could not stop service " << service_name;
      stopped_service_names.push_back(service_name);
    }
  }

  // Now all services running the test executable should be stopping, and can be
  // deleted.
  if (!WaitForProcessesStopped(kTestServiceExecutableName)) {
    return AssertionFailure()
           << "Not all " << kTestServiceExecutableName << " processes stopped";
  }
  // Issue an async DeleteService request for each service in parallel, then
  // wait for all of them to finish deleting.
  for (const base::string16& service_name : stopped_service_names) {
    if (!DeleteService(service_name.c_str()))
      return AssertionFailure() << "Could not delete service " << service_name;
  }
  for (const base::string16& service_name : stopped_service_names) {
    if (!WaitForServiceDeleted(service_name.c_str())) {
      return AssertionFailure()
             << "Did not finish deleting service " << service_name;
    }
  }
  return AssertionSuccess();
}

}  // namespace chrome_cleaner
