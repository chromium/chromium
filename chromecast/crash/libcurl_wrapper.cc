// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include <iostream>
#include <string>

#include "chromecast/crash/libcurl_wrapper.h"

namespace chromecast {
LibcurlWrapper::LibcurlWrapper()
    : init_ok_(false),
      curl_lib_(nullptr),
      last_curl_error_(""),
      curl_(nullptr),
      formpost_(nullptr),
      lastptr_(nullptr),
      headerlist_(nullptr) {}

LibcurlWrapper::~LibcurlWrapper() {
  if (init_ok_) {
    (*easy_cleanup_)(curl_);
    dlclose(curl_lib_);
  }
}

bool LibcurlWrapper::AddFile(const std::string& upload_file_path,
                             const std::string& basename) {
  if (!CheckInit())
    return false;

  std::cout << "Adding " << upload_file_path << " to form upload.";
  // Add form file.
  (*formadd_)(&formpost_, &lastptr_, CURLFORM_COPYNAME, basename.c_str(),
              CURLFORM_FILE, upload_file_path.c_str(), CURLFORM_END);

  return true;
}

// Callback to get the response data from server.
static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userp) {
  if (!userp)
    return 0;

  std::string* response = reinterpret_cast<std::string*>(userp);
  size_t real_size = size * nmemb;
  response->append(reinterpret_cast<char*>(ptr), real_size);
  return real_size;
}

bool LibcurlWrapper::SendRequest(
    const std::string& url,
    const std::map<std::string, std::string>& parameters,
    long* http_status_code,
    std::string* http_header_data,
    std::string* http_response_data) {
  if (!CheckInit())
    return false;

  std::map<std::string, std::string>::const_iterator iter = parameters.begin();
  for (; iter != parameters.end(); ++iter)
    (*formadd_)(&formpost_, &lastptr_, CURLFORM_COPYNAME, iter->first.c_str(),
                CURLFORM_COPYCONTENTS, iter->second.c_str(), CURLFORM_END);

  (*easy_setopt_)(curl_, CURLOPT_HTTPPOST, formpost_);

  return SendRequestInner(url, http_status_code, http_header_data,
                          http_response_data);
}

bool LibcurlWrapper::Init() {
  // First check to see if libcurl was statically linked:
  curl_lib_ = dlopen(nullptr, RTLD_NOW);
  if (curl_lib_ && (!dlsym(curl_lib_, "curl_easy_init") ||
                    !dlsym(curl_lib_, "curl_easy_setopt"))) {
    // Not statically linked, try again below.
    dlerror();  // Clear dlerror before attempting to open libraries.
    dlclose(curl_lib_);
    curl_lib_ = nullptr;
  }
  if (!curl_lib_) {
    curl_lib_ = dlopen("libcurl.so", RTLD_NOW);
  }
  if (!curl_lib_) {
    curl_lib_ = dlopen("libcurl.so.4", RTLD_NOW);
  }
  if (!curl_lib_) {
    curl_lib_ = dlopen("libcurl.so.3", RTLD_NOW);
  }
  if (!curl_lib_) {
    std::cout << "Could not find libcurl via dlopen";
    return false;
  }

  if (!SetFunctionPointers()) {
    std::cout << "Could not find function pointers";
    return false;
  }

  curl_ = (*easy_init_)();

  last_curl_error_ = "No Error";

  if (!curl_) {
    dlclose(curl_lib_);
    std::cout << "Curl initialization failed";
    return false;
  }

  init_ok_ = true;
  return true;
}

#define SET_AND_CHECK_FUNCTION_POINTER(var, function_name, type)      \
  var = reinterpret_cast<type>(dlsym(curl_lib_, function_name));      \
  if (!var) {                                                         \
    std::cout << "Could not find libcurl function " << function_name; \
    init_ok_ = false;                                                 \
    return false;                                                     \
  }

bool LibcurlWrapper::SetFunctionPointers() {
  SET_AND_CHECK_FUNCTION_POINTER(easy_init_, "curl_easy_init", CURL * (*)());

  SET_AND_CHECK_FUNCTION_POINTER(easy_setopt_, "curl_easy_setopt",
                                 CURLcode(*)(CURL*, CURLoption, ...));

  SET_AND_CHECK_FUNCTION_POINTER(
      formadd_, "curl_formadd",
      CURLFORMcode(*)(curl_httppost**, curl_httppost**, ...));

  SET_AND_CHECK_FUNCTION_POINTER(slist_append_, "curl_slist_append",
                                 curl_slist * (*)(curl_slist*, const char*));

  SET_AND_CHECK_FUNCTION_POINTER(easy_perform_, "curl_easy_perform",
                                 CURLcode(*)(CURL*));

  SET_AND_CHECK_FUNCTION_POINTER(easy_cleanup_, "curl_easy_cleanup",
                                 void (*)(CURL*));

  SET_AND_CHECK_FUNCTION_POINTER(easy_getinfo_, "curl_easy_getinfo",
                                 CURLcode(*)(CURL*, CURLINFO info, ...));

  SET_AND_CHECK_FUNCTION_POINTER(easy_reset_, "curl_easy_reset",
                                 void (*)(CURL*));

  SET_AND_CHECK_FUNCTION_POINTER(slist_free_all_, "curl_slist_free_all",
                                 void (*)(curl_slist*));

  SET_AND_CHECK_FUNCTION_POINTER(formfree_, "curl_formfree",
                                 void (*)(curl_httppost*));
  return true;
}

bool LibcurlWrapper::SendRequestInner(const std::string& url,
                                      long* http_status_code,
                                      std::string* http_header_data,
                                      std::string* http_response_data) {
  std::string url_copy(url);
  (*easy_setopt_)(curl_, CURLOPT_URL, url_copy.c_str());
  // Use the enum when available in the header file.
  (*easy_setopt_)(curl_, CURLOPT_SSLVERSION, 6 /*CURL_SSLVERSION_TLSv1_2*/);

  // Disable 100-continue header.
  char buf[] = "Expect:";
  headerlist_ = (*slist_append_)(headerlist_, buf);
  (*easy_setopt_)(curl_, CURLOPT_HTTPHEADER, headerlist_);

  if (http_response_data != nullptr) {
    http_response_data->clear();
    (*easy_setopt_)(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    (*easy_setopt_)(curl_, CURLOPT_WRITEDATA,
                    reinterpret_cast<void*>(http_response_data));
  }
  if (http_header_data != nullptr) {
    http_header_data->clear();
    (*easy_setopt_)(curl_, CURLOPT_HEADERFUNCTION, WriteCallback);
    (*easy_setopt_)(curl_, CURLOPT_HEADERDATA,
                    reinterpret_cast<void*>(http_header_data));
  }
  CURLcode err_code = CURLE_OK;
  err_code = (*easy_perform_)(curl_);
  easy_strerror_ = reinterpret_cast<const char* (*)(CURLcode)>(
      dlsym(curl_lib_, "curl_easy_strerror"));

  if (http_status_code != nullptr) {
    (*easy_getinfo_)(curl_, CURLINFO_RESPONSE_CODE, http_status_code);
  }

#ifndef NDEBUG
  if (err_code != CURLE_OK)
    fprintf(stderr, "Failed to send http request to %s, error: %s\n",
            url.c_str(), (*easy_strerror_)(err_code));
#endif

  Reset();

  return err_code == CURLE_OK;
}

void LibcurlWrapper::Reset() {
  if (headerlist_ != nullptr) {
    (*slist_free_all_)(headerlist_);
    headerlist_ = nullptr;
  }

  if (formpost_ != nullptr) {
    (*formfree_)(formpost_);
    formpost_ = nullptr;
  }

  (*easy_reset_)(curl_);
}

bool LibcurlWrapper::CheckInit() {
  if (!init_ok_) {
    std::cout << "LibcurlWrapper: You must call Init(), and have it return "
                 "'true' before invoking any other methods.\n";
    return false;
  }

  return true;
}

}  // namespace chromecast
