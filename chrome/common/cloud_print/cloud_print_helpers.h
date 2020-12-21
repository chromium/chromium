// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
#define CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

class GURL;

namespace base {
class Value;
}

// Helper consts and methods for both cloud print and chrome browser.
namespace cloud_print {

// A map representing printer tags.
typedef std::map<std::string, std::string> PrinterTags;

// Appends a relative path to the url making sure to append a '/' if the
// URL's path does not end with a slash. It is assumed that |path| does not
// begin with a '/'.
// NOTE: Since we ALWAYS want to append here, we simply append the path string
// instead of calling url::ResolveRelative. The input |url| may or may not
// contain a '/' at the end.
std::string AppendPathToUrl(const GURL& url, const std::string& path);

GURL GetUrlForSearch(const GURL& cloud_print_server_url);
GURL GetUrlForSubmit(const GURL& cloud_print_server_url);
GURL GetUrlForPrinterList(const GURL& cloud_print_server_url,
                          const std::string& proxy_id);
GURL GetUrlForPrinterRegistration(const GURL& cloud_print_server_url);
GURL GetUrlForPrinterUpdate(const GURL& cloud_print_server_url,
                            const std::string& printer_id);
GURL GetUrlForPrinterDelete(const GURL& cloud_print_server_url,
                            const std::string& printer_id,
                            const std::string& reason);
GURL GetUrlForJobFetch(const GURL& cloud_print_server_url,
                       const std::string& printer_id,
                       const std::string& reason);
GURL GetUrlForJobCjt(const GURL& cloud_print_server_url,
                     const std::string& job_id,
                     const std::string& reason);
GURL GetUrlForJobDelete(const GURL& cloud_print_server_url,
                        const std::string& job_id);
GURL GetUrlForJobStatusUpdate(const GURL& cloud_print_server_url,
                              const std::string& job_id,
                              const std::string& status_string,
                              int connector_code);
GURL GetUrlForUserMessage(const GURL& cloud_print_server_url,
                          const std::string& message_id);
GURL GetUrlForGetAuthCode(const GURL& cloud_print_server_url,
                          const std::string& oauth_client_id,
                          const std::string& proxy_id);

// Parses the response data for any cloud print server request. The method
// returns none Value if there was an error in parsing the JSON. The |succeeded|
// parameters returns the value of the "success" value in the response JSON.
// Returns the response as a dictionary value on success.
base::Value ParseResponseJSON(const std::string& response_data,
                              bool* succeeded);

// Returns the MIME type of multipart with |mime_boundary|.
std::string GetMultipartMimeType(const std::string& mime_boundary);

// Returns an MD5 hash for |printer_tags| and the default required tags.
std::string GetHashOfPrinterTags(const PrinterTags& printer_tags);

// Returns the post data for |printer_tags| and the default required tags.
std::string GetPostDataForPrinterTags(
    const PrinterTags& printer_tags,
    const std::string& mime_boundary,
    const std::string& proxy_tag_prefix,
    const std::string& tags_hash_tag_name);

// Get the cloud print auth header value from |auth_token|.
std::string GetCloudPrintAuthHeaderValue(const std::string& auth_token);

}  // namespace cloud_print

#endif  // CHROME_COMMON_CLOUD_PRINT_CLOUD_PRINT_HELPERS_H_
