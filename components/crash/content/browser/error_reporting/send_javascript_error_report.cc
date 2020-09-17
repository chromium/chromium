// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/error_reporting/send_javascript_error_report.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/core/app/client_upload_info.h"
#include "components/feedback/redaction_tool.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)
constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";
#else
constexpr char kCrashEndpointUrl[] = "";
#endif

std::string& GetCrashEndpoint() {
  static base::NoDestructor<std::string> crash_endpoint(kCrashEndpointUrl);
  return *crash_endpoint;
}

struct OsVersionOverride {
  OsVersionOverride(int32_t major_override,
                    int32_t minor_override,
                    int32_t bugfix_override)
      : major(major_override), minor(minor_override), bugfix(bugfix_override) {}
  int32_t major;
  int32_t minor;
  int32_t bugfix;
};

// If return value is set, use that as the major/minor/bugfix OS version
// numbers. This is used as dependency injection during testing.
base::Optional<OsVersionOverride>& GetOsVersionOverrides() {
  static base::NoDestructor<base::Optional<OsVersionOverride>> testing_override;
  return *testing_override;
}

// TODO(crbug.com/1129544) This is currently disabled due to Windows DLL
// thunking issues. Fix & re-enable.
#if !defined(OS_WIN)

void OnRequestComplete(std::unique_ptr<network::SimpleURLLoader> url_loader,
                       base::ScopedClosureRunner callback_runner,
                       std::unique_ptr<std::string> response_body) {
  if (response_body) {
    // TODO(iby): Update the crash log (uploads.log)
    DVLOG(1) << "Uploaded crash report. ID: " << *response_body;
  } else {
    LOG(ERROR) << "Failed to upload crash report";
  }
  // callback_runner will implicitly run the callback when we reach this line.
}

// Sometimes, the stack trace will contain an error message as the first line,
// which confuses the Crash server. This function deletes it if it is present.
void RemoveErrorMessageFromStackTrace(const std::string& error_message,
                                      std::string& stack_trace) {
  // Keep the original stack trace if the error message is not present.
  const auto error_message_index = stack_trace.find(error_message);
  if (error_message_index == std::string::npos) {
    return;
  }

  // If the stack trace only contains one line, then delete the whole trace.
  const auto first_line_end_index = stack_trace.find('\n');
  if (first_line_end_index == std::string::npos) {
    stack_trace.clear();
    return;
  }

  // Otherwise, delete the first line.
  stack_trace = stack_trace.substr(first_line_end_index + 1);
}

std::string RedactErrorMessage(const std::string& message) {
  return feedback::RedactionTool(/*first_party_extension_ids=*/nullptr)
      .Redact(message);
}

// Returns the redacted, fixed-up error report if the user consented to have it
// sent. Returns base::nullopt if the user did not consent or we otherwise
// should not send the report. All the MayBlock work should be done in here.
base::Optional<JavaScriptErrorReport> CheckConsentAndRedact(
    JavaScriptErrorReport error_report) {
  if (!crash_reporter::GetClientCollectStatsConsent()) {
    return base::nullopt;
  }

  // Remove error message from stack trace before redaction, since redaction
  // might change the error message enough that we don't find it.
  if (error_report.stack_trace) {
    RemoveErrorMessageFromStackTrace(error_report.message,
                                     *error_report.stack_trace);
  }

  error_report.message = RedactErrorMessage(error_report.message);
  // TODO(https://crbug.com/1121816): Also redact stack trace, but don't
  // completely remove the URL (only query & fragment).
  return error_report;
}

using ParameterMap = std::map<std::string, std::string>;

std::string BuildPostRequestQueryString(const ParameterMap& params) {
  std::vector<std::string> query_parts;
  for (const auto& kv : params) {
    query_parts.push_back(base::StrCat(
        {kv.first, "=",
         net::EscapeQueryParamValue(kv.second, /*use_plus=*/false)}));
  }
  return base::JoinString(query_parts, "&");
}

struct PlatformInfo {
  std::string product_name;
  std::string version;
  std::string channel;
  std::string os_version;
};

PlatformInfo GetPlatformInfo() {
  PlatformInfo info;

  // TODO(https://crbug.com/1121816): Get correct product_name for non-POSIX
  // platforms.
#if defined(OS_POSIX) && !defined(OS_APPLE)
  crash_reporter::GetClientProductNameAndVersion(&info.product_name,
                                                 &info.version, &info.channel);
#endif
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  const base::Optional<OsVersionOverride>& version_override =
      GetOsVersionOverrides();
  if (version_override) {
    os_major_version = version_override->major;
    os_minor_version = version_override->minor;
    os_bugfix_version = version_override->bugfix;
  } else {
    base::SysInfo::OperatingSystemVersionNumbers(
        &os_major_version, &os_minor_version, &os_bugfix_version);
  }

  info.os_version = base::StringPrintf("%d.%d.%d", os_major_version,
                                       os_minor_version, os_bugfix_version);
  return info;
}

void SendReport(const GURL& url,
                const std::string& body,
                base::ScopedClosureRunner callback_runner,
                network::SharedURLLoaderFactory* loader_factory) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = url;

  const auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("javascript_report_error", R"(
      semantics {
        sender: "JavaScript error reporter"
        description:
          "Chrome can send JavaScript errors that occur within built-in "
          "component extensions. If enabled, the error message, along "
          "with information about Chrome and the operating system, is sent to "
          "Google."
        trigger:
          "A JavaScript error occurs in a Chrome component extension (an "
          "extension bundled with the Chrome browser, not downloaded "
          "separately)."
        data:
          "The JavaScript error message, the version and channel of Chrome, "
          "the URL of the extension, the line and column number where the "
          "error occurred, and a stack trace of the error."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can enable or disable this feature via 'Automatically send "
          "usage statistics and crash reports to Google' in Chromium's "
          "settings under Advanced, Privacy. (This is in System Settings on "
          "Chromebooks.) This feature is enabled by default."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: false
          }
        }
      })");

  DVLOG(1) << "Sending crash report: " << resource_request->url;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (!body.empty()) {
    url_loader->AttachStringForUpload(body, "text/plain");
  }

  constexpr int kCrashEndpointResponseMaxSizeInBytes = 1024;
  network::SimpleURLLoader* loader = url_loader.get();
  loader->DownloadToString(
      loader_factory,
      base::BindOnce(&OnRequestComplete, std::move(url_loader),
                     std::move(callback_runner)),
      kCrashEndpointResponseMaxSizeInBytes);
}

// Finishes sending process once the MayBlock processing is done. On UI thread.
void OnConsentCheckCompleted(
    base::ScopedClosureRunner callback_runner,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::Optional<JavaScriptErrorReport> error_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_report) {
    // User didn't consent. This isn't an error so don't log an error.
    return;
  }

  std::string& crash_endpoint_string = GetCrashEndpoint();
  if (crash_endpoint_string.empty()) {
    LOG(WARNING) << "Not sending error reports to Google for browsers that are "
                    "not Google Chrome";
    return;
  }

  // TODO(https://crbug.com/986166): Use crash_reporter for Chrome OS.
  const auto platform = GetPlatformInfo();

  const GURL source(error_report->url);
  const auto product = error_report->product.empty() ? platform.product_name
                                                     : error_report->product;
  const auto version =
      error_report->version.empty() ? platform.version : error_report->version;

  ParameterMap params;
  params["prod"] = net::EscapeQueryParamValue(product, /*use_plus=*/false);
  params["ver"] = net::EscapeQueryParamValue(version, /*use_plus=*/false);
  params["type"] = "JavascriptError";
  params["error_message"] = error_report->message;
  params["browser"] = "Chrome";
  params["browser_version"] = platform.version;
  params["channel"] = platform.channel;
  // TODO(https://crbug.com/1121816): Handle non-ChromeOS platforms.
  params["os"] = "ChromeOS";
  params["os_version"] = platform.os_version;
  params["full_url"] = source.spec();
  params["url"] = source.path();
  params["src"] = source.spec();
  if (error_report->line_number)
    params["line"] = base::NumberToString(*error_report->line_number);
  if (error_report->column_number)
    params["column"] = base::NumberToString(*error_report->column_number);

  const GURL url(base::StrCat(
      {crash_endpoint_string, "?", BuildPostRequestQueryString(params)}));
  std::string body;
  if (error_report->stack_trace) {
    body = std::move(*error_report->stack_trace);
  }

  SendReport(url, body, std::move(callback_runner), loader_factory.get());
}

#endif  // !defined(OS_WIN)

}  // namespace

// TODO(crbug.com/1129544) This is currently disabled due to Windows DLL
// thunking issues. Fix & re-enable.
#if !defined(OS_WIN)

void SendJavaScriptErrorReport(JavaScriptErrorReport error_report,
                               base::OnceClosure completion_callback,
                               content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ScopedClosureRunner callback_runner(std::move(completion_callback));

  // loader_factory must be created on UI thread. Get it now while we still
  // know the browser_context pointer is valid.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context)
          ->GetURLLoaderFactoryForBrowserProcess();

  // Consent check needs to be done on a blockable thread. We must return to
  // this thread (the UI thread) to use the loader_factory.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckConsentAndRedact, std::move(error_report)),
      base::BindOnce(&OnConsentCheckCompleted, std::move(callback_runner),
                     std::move(loader_factory)));
}

#endif  // !defined(OS_WIN)

void SetCrashEndpointForTesting(const std::string& endpoint) {
  GetCrashEndpoint() = endpoint;
}

// The weird "{" comment is to get the
// CheckNoProductionCodeUsingTestOnlyFunctions PRESUBMIT to be quiet.
void SetOsVersionForTesting(int32_t os_major_version,  // {
                            int32_t os_minor_version,
                            int32_t os_bugfix_version) {
  GetOsVersionOverrides().emplace(os_major_version, os_minor_version,
                                  os_bugfix_version);
}

void ClearOsVersionTestingOverride() {
  GetOsVersionOverrides().reset();
}
