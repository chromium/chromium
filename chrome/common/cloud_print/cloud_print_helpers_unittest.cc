// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/cloud_print/cloud_print_helpers.h"

#include "base/hash/md5.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/common/channel_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cloud_print {

namespace {

void CheckURLs(const GURL& server_base_url) {
  std::string expected_url_base = server_base_url.spec();
  if (expected_url_base.back() != '/')
    expected_url_base += "/";

  EXPECT_EQ(base::StringPrintf("%ssearch", expected_url_base.c_str()),
            GetUrlForSearch(server_base_url).spec());

  EXPECT_EQ(base::StringPrintf("%ssubmit", expected_url_base.c_str()),
            GetUrlForSubmit(server_base_url).spec());

  EXPECT_EQ(base::StringPrintf("%slist?proxy=demoproxy",
                expected_url_base.c_str()),
            GetUrlForPrinterList(
                server_base_url, std::string("demoproxy")).spec());

  EXPECT_EQ(base::StringPrintf("%sregister", expected_url_base.c_str()),
            GetUrlForPrinterRegistration(server_base_url).spec());

  EXPECT_EQ(base::StringPrintf("%supdate?printerid=printeridfoo",
                expected_url_base.c_str()),
            GetUrlForPrinterUpdate(server_base_url, "printeridfoo").spec());

  EXPECT_EQ(base::StringPrintf("%sdelete?printerid=printeridbar&reason=deleted",
                expected_url_base.c_str()),
            GetUrlForPrinterDelete(
                server_base_url, "printeridbar", "deleted").spec());

  EXPECT_EQ(base::StringPrintf("%sfetch?printerid=myprinter&deb=nogoodreason",
                expected_url_base.c_str()),
            GetUrlForJobFetch(
                server_base_url, "myprinter", "nogoodreason").spec());

  EXPECT_EQ(base::StringPrintf("%sdeletejob?jobid=myprinter",
                expected_url_base.c_str()),
            GetUrlForJobDelete(server_base_url, "myprinter").spec());

  EXPECT_EQ(base::StringPrintf(
                "%scontrol?jobid=myprinter&status=s1&connector_code=0",
                expected_url_base.c_str()),
            GetUrlForJobStatusUpdate(
                server_base_url, "myprinter", "s1", 0).spec());

  EXPECT_EQ(base::StringPrintf("%smessage?code=testmsg",
                expected_url_base.c_str()),
            GetUrlForUserMessage(server_base_url, "testmsg").spec());

  EXPECT_EQ(base::StringPrintf(
                "%screaterobot?oauth_client_id=democlientid&proxy=demoproxy",
                expected_url_base.c_str()),
            GetUrlForGetAuthCode(
                server_base_url, "democlientid", "demoproxy").spec());
}

}  // namespace

TEST(CloudPrintHelpersTest, GetURLs) {
  CheckURLs(GURL("https://www.google.com/cloudprint"));
  CheckURLs(GURL("https://www.google.com/cloudprint/"));
  CheckURLs(GURL("http://www.myprinterserver.com"));
  CheckURLs(GURL("http://www.myprinterserver.com/"));
}

TEST(CloudPrintHelpersTest, GetHashOfPrinterTags) {
  PrinterTags printer_tags;
  printer_tags["tag1"] = std::string("value1");
  printer_tags["tag2"] = std::string("value2");

  std::string expected_list_string = base::StringPrintf(
      "chrome_version%ssystem_name%ssystem_version%stag1value1tag2value2",
      chrome::GetVersionString().c_str(),
      base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str());
  EXPECT_EQ(base::MD5String(expected_list_string),
            GetHashOfPrinterTags(printer_tags));
}

TEST(CloudPrintHelpersTest, GetPostDataForPrinterTags) {
  PrinterTags printer_tags;
  printer_tags["tag1"] = std::string("value1");
  printer_tags["tag2"] = std::string("value2");

  std::string expected = base::StringPrintf(
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__chrome_version=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__system_name=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__system_version=%s\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__tag1=value1\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__tag2=value2\r\n"
      "--test_mime_boundary\r\nContent-Disposition: form-data; name=\"tag\""
      "\r\n\r\n__test__tagshash=%s\r\n",
      chrome::GetVersionString().c_str(),
      base::SysInfo::OperatingSystemName().c_str(),
      base::SysInfo::OperatingSystemVersion().c_str(),
      GetHashOfPrinterTags(printer_tags).c_str());

  EXPECT_EQ(expected, GetPostDataForPrinterTags(
      printer_tags,
      std::string("test_mime_boundary"),
      std::string("__test__"),
      std::string("__test__tagshash")));
}

TEST(CloudPrintHelpersTest, GetCloudPrintAuthHeader) {
  std::string test_auth("testauth");
  EXPECT_EQ("Authorization: OAuth testauth",
            GetCloudPrintAuthHeader(test_auth));
}

}  // namespace cloud_print
