// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/cloud_print/cloud_print_service_helpers.h"

#include "base/hash/md5.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/common/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cloud_print {

namespace {

void CheckJobStatusURLs(const GURL& server_base_url) {
  std::string expected_url_base = server_base_url.spec();
  if (expected_url_base.back() != '/')
    expected_url_base += "/";

  EXPECT_EQ(base::StringPrintf(
                "%scontrol?jobid=87654321&status=ERROR&connector_code=1",
                expected_url_base.c_str()),
            GetUrlForJobStatusUpdate(server_base_url, "87654321",
                PRINT_JOB_STATUS_ERROR, 1).spec());

  PrintJobDetails details;
  details.status = PRINT_JOB_STATUS_IN_PROGRESS;
  details.platform_status_flags = 2;
  details.status_message = "Out of Paper";
  details.total_pages = 345;
  details.pages_printed = 47;
  EXPECT_EQ(base::StringPrintf(
                "%scontrol?jobid=87654321&status=IN_PROGRESS&code=2"
                "&message=Out%%20of%%20Paper&numpages=345&pagesprinted=47",
                expected_url_base.c_str()),
            GetUrlForJobStatusUpdate(
                server_base_url, "87654321", details).spec());
}

}  // namespace

TEST(CloudPrintServiceHelpersTest, GetURLs) {
  CheckJobStatusURLs(GURL("https://www.google.com/cloudprint"));
  CheckJobStatusURLs(GURL("https://www.google.com/cloudprint/"));
  CheckJobStatusURLs(GURL("http://www.myprinterserver.com"));
  CheckJobStatusURLs(GURL("http://www.myprinterserver.com/"));
}

TEST(CloudPrintServiceHelpersTest, GetHashOfPrinterInfo) {
  printing::PrinterBasicInfo printer_info;
  printer_info.options["tag1"] = std::string("value1");
  printer_info.options["tag2"] = std::string("value2");

  std::string expected_list_string = base::StringPrintf(
      "chrome_version%ssystem_name%ssystem_version%stag1value1tag2value2",
      chrome::GetVersionString().c_str(),
      base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str());
  EXPECT_EQ(base::MD5String(expected_list_string),
            GetHashOfPrinterInfo(printer_info));
}

TEST(CloudPrintServiceHelpersTest, GetPostDataForPrinterInfo) {
  printing::PrinterBasicInfo printer_info;
  printer_info.options["tag1"] = std::string("value1");
  printer_info.options["tag2"] = std::string("value2");

  std::string expected = base::StringPrintf(
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__chrome_version=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__system_name=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__system_version=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__tag1=value1\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__tag2=value2\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__cp__tagshash=%s\r\n",
      chrome::GetVersionString().c_str(),
      base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      GetHashOfPrinterInfo(printer_info).c_str());

  EXPECT_EQ(expected, GetPostDataForPrinterInfo(
      printer_info, std::string("test_mime_boundary")));
}

TEST(CloudPrintServiceHelpersTest, IsDryRunJob) {
  std::vector<std::string> tags_not_dry_run;
  tags_not_dry_run.push_back("tag_1");
  EXPECT_FALSE(IsDryRunJob(tags_not_dry_run));

  std::vector<std::string> tags_dry_run;
  tags_dry_run.push_back("__cp__dry_run");
  EXPECT_TRUE(IsDryRunJob(tags_dry_run));
}

}  // namespace cloud_print

