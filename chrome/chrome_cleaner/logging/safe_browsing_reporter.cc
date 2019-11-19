// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/logging/safe_browsing_reporter.h"

#include <windows.h>

#include <iphlpapi.h>
#include <stdint.h>
#include <wininet.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/constants/chrome_cleaner_switches.h"
#include "chrome/chrome_cleaner/http/http_agent.h"
#include "chrome/chrome_cleaner/http/http_response.h"
#include "chrome/chrome_cleaner/http/http_status_codes.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "url/gurl.h"

namespace chrome_cleaner {

namespace {

// Number of times that an upload will be retried on failure.
const unsigned int kMaxUploadAttempts = 3;

// How long to wait (in seconds) before every upload attempt. The first delay
// should be zero.
const unsigned int kUploadAttemptDelaySeconds[kMaxUploadAttempts] = {0, 5, 300};

// How long to wait for network changes (total in seconds), if Safe Browsing is
// not reachable.
const unsigned int kNetworkPresenceTimeoutSeconds = 300;

// The maximum timeout we allow, in milliseconds.
const DWORD kMaxTimeoutMilliseconds = 600000;

// Extracts data from |http_response| and returns it as a std::string.
std::string GetHttpResponseData(chrome_cleaner::HttpResponse* http_response) {
  std::string response_data;
  while (true) {
    char buffer[8192] = {};
    uint32_t count = static_cast<uint32_t>(base::size(buffer));
    if (!http_response->ReadData(buffer, &count)) {
      LOG(ERROR) << "ReadData failed";
      break;
    } else if (!count) {
      break;
    }
    response_data.append(buffer, count);
  }
  return response_data;
}

// Returns the URL that logs should be uploaded to.
GURL GetSafeBrowsingReportUrl(const std::string& default_url) {
  GURL upload_url(default_url);

  std::string test_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kTestLoggingURLSwitch);
  if (!test_url.empty()) {
    LOG(INFO) << "Using test logging url: " << test_url;
    upload_url = GURL(test_url);
  }

  DCHECK(upload_url.is_valid());
  return upload_url;
}

class NetworkCheckerImpl : public NetworkChecker {
 public:
  NetworkCheckerImpl() = default;
  ~NetworkCheckerImpl() override = default;

  // TODO(olivierli) Make upload_url a member variable
  bool IsSafeBrowsingReachable(const GURL& upload_url) const override {
    const BOOL is_reachable =
        ::InternetCheckConnection(base::UTF8ToWide(upload_url.spec()).c_str(),
                                  FLAG_ICC_FORCE_CONNECTION, 0);
    if (is_reachable == FALSE)
      PLOG(INFO) << "Safe Browsing is not reachable";
    return !!is_reachable;
  }

  bool WaitForSafeBrowsing(const GURL& upload_url,
                           const base::TimeDelta& wait_time) override {
    const base::Time start_time = base::Time::Now();
    if (wait_time <= base::TimeDelta()) {
      LOG(INFO) << "Past deadline, not trying to wait for Safe Browsing";
      return IsSafeBrowsingReachable(upload_url);
    }

    HANDLE event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
      PLOG(ERROR) << "Unable to create event";
      return false;
    }
    base::ScopedClosureRunner close_event(
        base::BindRepeating(base::IgnoreResult(&::CloseHandle), event));

    OVERLAPPED overlapped = {0};
    overlapped.hEvent = event;

    HANDLE handle = nullptr;
    DWORD ret = ::NotifyAddrChange(&handle, &overlapped);
    if (ret != ERROR_IO_PENDING) {
      PLOG(ERROR) << "Error in NotifyAddrChange (" << ret << ")";
      return false;
    }
    base::ScopedClosureRunner cancel_ip_change_notify(base::BindRepeating(
        base::IgnoreResult(&::CancelIPChangeNotify), &overlapped));

    // NotifyAddrChange will only notify when there are address changes to the
    // network interfaces, so avoid a race condition and make sure that Safe
    // Browsing is in fact not reachable before waiting for address changes.
    if (IsSafeBrowsingReachable(upload_url)) {
      LOG(INFO) << "Safe Browsing is reachable, not waiting";
      return true;
    }

    EnsureCancelWaitEventCreated();
    HANDLE event_list[2] = {overlapped.hEvent, cancel_wait_event_.Get()};

    const DWORD timeout_ms = static_cast<DWORD>(wait_time.InMilliseconds());
    DCHECK_LT(timeout_ms, kMaxTimeoutMilliseconds)
        << "timeout should be a reasonable value";
    LOG(INFO) << "Waiting up to " << timeout_ms << "ms for Internet connection";
    ret = ::WaitForMultipleObjects(2, event_list, FALSE, timeout_ms);
    if (ret == WAIT_OBJECT_0) {
      // The event was signaled by NotifyAddrChange. Cancel the change
      // notification so we don't receive notifications on this particular
      // OVERLAPPED structure again.
      cancel_ip_change_notify.RunAndReset();
      if (IsSafeBrowsingReachable(upload_url)) {
        LOG(INFO) << "Safe Browsing now reachable";
        return true;
      }
    } else if (ret == (WAIT_OBJECT_0 + 1)) {
      LOG(INFO) << "Wait canceled";
      return false;
    } else {
      LOG(INFO) << "WaitForMultipleObjects returned " << ret;
    }

    LOG(INFO) << "Trying to wait for Safe Browsing to be reachable again";

    // TODO(csharp): In cases where we timed out above. this recursive call
    //               will immediately return because (start_time + wait_time)
    //               - base::Time::Now() should ~0. This may or may not be
    //               intended. Refactor this to not use recursion to make it
    //               clearer how this is supposed to work.
    return WaitForSafeBrowsing(upload_url,
                               (start_time + wait_time) - base::Time::Now());
  }

  void CancelWaitForShutdown() override {
    EnsureCancelWaitEventCreated();
    ::SetEvent(cancel_wait_event_.Get());
  }

 private:
  void EnsureCancelWaitEventCreated() {
    base::AutoLock lock(cancel_wait_event_lock_);
    if (!cancel_wait_event_.IsValid())
      cancel_wait_event_.Set(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
  }

  // Manual-reset event, which will remain set once set.
  base::win::ScopedHandle cancel_wait_event_;
  base::Lock cancel_wait_event_lock_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCheckerImpl);
};

NetworkChecker* current_network_checker{nullptr};
const HttpAgentFactory* current_http_agent_factory{nullptr};

NetworkChecker* GetNetworkChecker() {
  // This is "leaked" on purpose to avoid static destruction order woes.
  // Neither NetworkChecker nor its parent classes dtors do any work.
  static NetworkChecker* network_checker = new NetworkCheckerImpl();

  if (!current_network_checker) {
    current_network_checker = network_checker;
  }

  return current_network_checker;
}

const HttpAgentFactory* GetHttpAgentFactory() {
  // This is "leaked" on purpose to avoid static destruction order woes.
  // Neither HttpAgentFactory nor its parent classes dtors do any work.
  static HttpAgentFactory* http_agent_factory = new HttpAgentFactory();

  if (!current_http_agent_factory) {
    current_http_agent_factory = http_agent_factory;
  }

  return current_http_agent_factory;
}

}  // namespace

base::RepeatingCallback<void(base::TimeDelta)>
    SafeBrowsingReporter::sleep_callback_(
        base::BindRepeating(&base::PlatformThread::Sleep));

SafeBrowsingReporter::~SafeBrowsingReporter() = default;

// static
void SafeBrowsingReporter::SetHttpAgentFactoryForTesting(
    const HttpAgentFactory* factory) {
  current_http_agent_factory = factory;
}

// static
void SafeBrowsingReporter::SetSleepCallbackForTesting(
    base::RepeatingCallback<void(base::TimeDelta)> callback) {
  sleep_callback_ = callback;
}

// static
void SafeBrowsingReporter::SetNetworkCheckerForTesting(
    NetworkChecker* checker) {
  current_network_checker = checker;
}

// static
void SafeBrowsingReporter::UploadReport(
    const OnResultCallback& done_callback,
    const std::string& default_url,
    const std::string& report,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  // SafeBrowsingReporter will PostTask to WorkerPool, giving that task
  // ownership of the object, which will get destructed on task completion.
  new SafeBrowsingReporter(done_callback, GetSafeBrowsingReportUrl(default_url),
                           report, traffic_annotation,
                           base::ThreadTaskRunnerHandle::Get());
}

// static
void SafeBrowsingReporter::CancelWaitForShutdown() {
  GetNetworkChecker()->CancelWaitForShutdown();
}

SafeBrowsingReporter::SafeBrowsingReporter(
    const OnResultCallback& done_callback,
    const GURL& upload_url,
    const std::string& serialized_report,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<base::TaskRunner> done_callback_runner)
    : upload_url_(upload_url),
      done_callback_runner_(done_callback_runner),
      done_callback_(done_callback) {
  DCHECK(done_callback_runner);
  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::MayBlock(),
                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                 base::BindRepeating(&SafeBrowsingReporter::UploadWithRetry,
                                     base::Owned(this), serialized_report,
                                     traffic_annotation));
}

void SafeBrowsingReporter::UploadWithRetry(
    const std::string& serialized_report,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  std::unique_ptr<ChromeFoilResponse> response(new ChromeFoilResponse);
  Result result = Result::UPLOAD_NO_NETWORK;
  if (GetNetworkChecker()->WaitForSafeBrowsing(
          upload_url_,
          base::TimeDelta::FromSeconds(kNetworkPresenceTimeoutSeconds))) {
    result = PerformUploadWithRetries(serialized_report, response.get(),
                                      traffic_annotation);
    if (result != Result::UPLOAD_SUCCESS) {
      LOG(WARNING) << "Failed to upload report to Safe Browsing API, "
                   << static_cast<int>(result);
    }
  }

  LOG(INFO) << "Calling done_callback_ with result: "
            << static_cast<int>(result);
  done_callback_runner_->PostTask(
      FROM_HERE, base::BindRepeating(done_callback_, result, serialized_report,
                                     base::Passed(&response)));
}

SafeBrowsingReporter::Result SafeBrowsingReporter::PerformUploadWithRetries(
    const std::string& serialized_report,
    ChromeFoilResponse* response,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(response);

  SafeBrowsingReporter::Result result = Result::UPLOAD_INTERNAL_ERROR;
  for (unsigned int attempt = 0; attempt < kMaxUploadAttempts; ++attempt) {
    sleep_callback_.Run(
        base::TimeDelta::FromSeconds(kUploadAttemptDelaySeconds[attempt]));
    result = PerformUpload(serialized_report, response, traffic_annotation);
    if (result == Result::UPLOAD_SUCCESS)
      break;
  }

  return result;
}

SafeBrowsingReporter::Result SafeBrowsingReporter::PerformUpload(
    const std::string& serialized_report,
    ChromeFoilResponse* response,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(response);

  // TODO(csharp): Add an entry point to the appspot test logging server to
  // generate errors, to replace this command line switch.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kForceLogsUploadFailureSwitch)) {
    LOG(WARNING) << "Forcing a logs upload failure.";
    return Result::UPLOAD_INTERNAL_ERROR;
  }

  std::unique_ptr<chrome_cleaner::HttpAgent> http_agent =
      GetHttpAgentFactory()->CreateHttpAgent();
  std::unique_ptr<chrome_cleaner::HttpResponse> http_response =
      http_agent->Post(base::UTF8ToWide(upload_url_.host()),
                       static_cast<uint16_t>(upload_url_.EffectiveIntPort()),
                       base::UTF8ToWide(upload_url_.PathForRequest()),
                       upload_url_.SchemeIsCryptographic(),
                       L"",  // No extra headers.
                       serialized_report, traffic_annotation);

  if (!http_response.get())
    return Result::UPLOAD_REQUEST_FAILED;

  Result result = Result::UPLOAD_INTERNAL_ERROR;

  uint16_t status_code = 0;
  if (http_response->GetStatusCode(&status_code)) {
    DCHECK_NE(status_code, 0);
    if (status_code == static_cast<uint16_t>(HttpStatus::kOk)) {
      std::string response_data = GetHttpResponseData(http_response.get());
      if (response->ParseFromString(response_data)) {
        result = Result::UPLOAD_SUCCESS;
        LOG(INFO) << "Upload response token: " << response->token();
      } else {
        result = Result::UPLOAD_INVALID_RESPONSE;
      }
    } else {
      LOG(WARNING) << "HttpResponse status: " << status_code;
      if (status_code ==
          static_cast<uint16_t>(HttpStatus::kRequestEntityTooLarge))
        result = Result::UPLOAD_ERROR_TOO_LARGE;
      else if (status_code == static_cast<uint16_t>(HttpStatus::kNotFound))
        result = Result::UPLOAD_REQUEST_FAILED;
    }
  } else {
    LOG(ERROR) << "Failed to retrieve data from HttpResponse";
  }

  return result;
}

}  // namespace chrome_cleaner
