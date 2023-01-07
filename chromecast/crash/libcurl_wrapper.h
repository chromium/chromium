// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LIBCURL_WRAPPER_H_
#define CHROMECAST_CRASH_LIBCURL_WRAPPER_H_

#include <map>
#include <string>

#include "third_party/breakpad/breakpad/src/third_party/curl/curl.h"

namespace chromecast {
class LibcurlWrapper {
 public:
  LibcurlWrapper();
  virtual ~LibcurlWrapper();
  virtual bool Init();
  virtual bool AddFile(const std::string& upload_file_path,
                       const std::string& basename);
  virtual bool SendRequest(const std::string& url,
                           const std::map<std::string, std::string>& parameters,
                           long* http_status_code,
                           std::string* http_header_data,
                           std::string* http_response_data);

 private:
  // This function initializes class state corresponding to function
  // pointers into the CURL library.
  bool SetFunctionPointers();

  bool SendRequestInner(const std::string& url,
                        long* http_status_code,
                        std::string* http_header_data,
                        std::string* http_response_data);

  void Reset();

  bool CheckInit();

  bool init_ok_;                 // Whether init succeeded
  void* curl_lib_;               // Pointer to result of dlopen() on
                                 // curl library
  std::string last_curl_error_;  // The text of the last error when
                                 // dealing
  // with CURL.

  CURL* curl_;  // Pointer for handle for CURL calls.

  CURL* (*easy_init_)(void);

  // Stateful pointers for calling into curl_formadd()
  struct curl_httppost* formpost_;
  struct curl_httppost* lastptr_;
  struct curl_slist* headerlist_;

  // Function pointers into CURL library
  CURLcode (*easy_setopt_)(CURL*, CURLoption, ...);
  CURLFORMcode (*formadd_)(struct curl_httppost**, struct curl_httppost**, ...);
  struct curl_slist* (*slist_append_)(struct curl_slist*, const char*);
  void (*slist_free_all_)(struct curl_slist*);
  CURLcode (*easy_perform_)(CURL*);
  const char* (*easy_strerror_)(CURLcode);
  void (*easy_cleanup_)(CURL*);
  CURLcode (*easy_getinfo_)(CURL*, CURLINFO info, ...);
  void (*easy_reset_)(CURL*);
  void (*formfree_)(struct curl_httppost*);
};
}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LIBCURL_WRAPPER_H_
