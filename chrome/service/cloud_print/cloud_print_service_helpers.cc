// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_service_helpers.h"

#include "base/strings/stringprintf.h"
#include "chrome/common/cloud_print/cloud_print_constants.h"
#include "chrome/common/cloud_print/cloud_print_helpers.h"
#include "chrome/service/cloud_print/cloud_print_token_store.h"
#include "chrome/service/service_process.h"

namespace {

std::string StringFromJobStatus(cloud_print::PrintJobStatus status) {
  std::string ret;
  switch (status) {
    case cloud_print::PRINT_JOB_STATUS_IN_PROGRESS:
      ret = "IN_PROGRESS";
      break;
    case cloud_print::PRINT_JOB_STATUS_ERROR:
      ret = "ERROR";
      break;
    case cloud_print::PRINT_JOB_STATUS_COMPLETED:
      ret = "DONE";
      break;
    default:
      ret = "UNKNOWN";
      NOTREACHED();
      break;
  }
  return ret;
}

}  // namespace

namespace cloud_print {

GURL GetUrlForJobStatusUpdate(const GURL& cloud_print_server_url,
                              const std::string& job_id,
                              PrintJobStatus status,
                              int connector_code) {
  return GetUrlForJobStatusUpdate(cloud_print_server_url,
                                  job_id,
                                  StringFromJobStatus(status),
                                  connector_code);
}

GURL GetUrlForJobStatusUpdate(const GURL& cloud_print_server_url,
                              const std::string& job_id,
                              const PrintJobDetails& details) {
  std::string status_string = StringFromJobStatus(details.status);
  std::string path(AppendPathToUrl(cloud_print_server_url, "control"));
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  std::string query =
      base::StringPrintf("jobid=%s&status=%s&code=%d&message=%s"
                         "&numpages=%d&pagesprinted=%d",
                         job_id.c_str(),
                         status_string.c_str(),
                         details.platform_status_flags,
                         details.status_message.c_str(),
                         details.total_pages,
                         details.pages_printed);
  replacements.SetQueryStr(query);
  return cloud_print_server_url.ReplaceComponents(replacements);
}

std::string GetHashOfPrinterInfo(
    const printing::PrinterBasicInfo& printer_info) {
  return GetHashOfPrinterTags(printer_info.options);
}

std::string GetPostDataForPrinterInfo(
    const printing::PrinterBasicInfo& printer_info,
    const std::string& mime_boundary) {
  return GetPostDataForPrinterTags(
      printer_info.options,
      mime_boundary,
      kCloudPrintServiceProxyTagPrefix,
      kCloudPrintServiceTagsHashTagName);
}

bool IsDryRunJob(const std::vector<std::string>& tags) {
  return base::Contains(tags, kCloudPrintServiceTagDryRunFlag);
}

std::string GetCloudPrintAuthHeaderFromStore() {
  CloudPrintTokenStore* token_store = CloudPrintTokenStore::current();
  if (!token_store || token_store->token().empty()) {
    // Using LOG here for critical errors. GCP connector may run in the headless
    // mode and error indication might be useful for user in that case.
    LOG(ERROR) << "CP_PROXY: Missing OAuth token for request";
    return std::string();
  }
  return GetCloudPrintAuthHeader(token_store->token());
}

}  // namespace cloud_print
