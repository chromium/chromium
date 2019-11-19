// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"

#if defined(USE_GOOGLE_API_KEYS_FOR_AUTH_KEY)
#include "google_apis/google_api_keys.h"
#endif

namespace data_reduction_proxy {
namespace {

std::string FormatOption(const std::string& name, const std::string& value) {
  return name + "=" + value;
}

bool ParseChromeProxyHeader(const net::HttpRequestHeaders& request_headers,
                            base::StringPairs* kv_pairs) {
  std::string chrome_proxy_header_value;
  return request_headers.GetHeader(chrome_proxy_header(),
                                   &chrome_proxy_header_value) &&
         base::SplitStringIntoKeyValuePairs(chrome_proxy_header_value,
                                            '=',  // Key-value delimiter
                                            ',',  // Key-value pair delimiter
                                            kv_pairs);
}

}  // namespace

const char kSecureSessionHeaderOption[] = "s";
const char kBuildNumberHeaderOption[] = "b";
const char kPatchNumberHeaderOption[] = "p";
const char kClientHeaderOption[] = "c";
const char kPageIdOption[] = "pid";

// The empty version for the authentication protocol. Currently used by
// Android webview.
#if defined(OS_ANDROID)
const char kAndroidWebViewProtocolVersion[] = "";
#endif

// static
bool DataReductionProxyRequestOptions::IsKeySetOnCommandLine() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kDataReductionProxyKey);
}

DataReductionProxyRequestOptions::DataReductionProxyRequestOptions(
    Client client,
    DataReductionProxyConfig* config)
    : DataReductionProxyRequestOptions(client,
                                       util::ChromiumVersion(),
                                       config) {}

DataReductionProxyRequestOptions::DataReductionProxyRequestOptions(
    Client client,
    const std::string& version,
    DataReductionProxyConfig* config)
    : client_(util::GetStringForClient(client)),
      server_experiments_(params::GetDataSaverServerExperiments()),
      data_reduction_proxy_config_(config),
      current_page_id_(base::RandUint64()) {
  DCHECK(data_reduction_proxy_config_);
  util::GetChromiumBuildAndPatch(version, &build_, &patch_);
}

DataReductionProxyRequestOptions::~DataReductionProxyRequestOptions() {
}

void DataReductionProxyRequestOptions::Init() {
  DCHECK(thread_checker_.CalledOnValidThread());
  key_ = GetDefaultKey(),
  UpdateCredentials();
  RegenerateRequestHeaderValue();
  // Called on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

std::string DataReductionProxyRequestOptions::GetHeaderValueForTesting() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return header_value_;
}

// static
void DataReductionProxyRequestOptions::AddPageIDRequestHeader(
    net::HttpRequestHeaders* request_headers,
    uint64_t page_id) {
  std::string header_value;
  if (request_headers->HasHeader(chrome_proxy_header())) {
    request_headers->GetHeader(chrome_proxy_header(), &header_value);
    request_headers->RemoveHeader(chrome_proxy_header());
    header_value += ", ";
  }
  // 64 bit uint fits in 17 characters when represented in hexadecimal,
  // including trailing null terminated character.
  char page_id_buffer[17];
  if (base::strings::SafeSPrintf(page_id_buffer, "%x", page_id) > 0) {
    header_value += FormatOption(kPageIdOption, page_id_buffer);
  }
  uint64_t page_id_tested;
  DCHECK(base::HexStringToUInt64(page_id_buffer, &page_id_tested) &&
         page_id_tested == page_id);
  ALLOW_UNUSED_LOCAL(page_id_tested);
  request_headers->SetHeader(chrome_proxy_header(), header_value);
}

void DataReductionProxyRequestOptions::AddRequestHeader(
    net::HttpRequestHeaders* request_headers,
    base::Optional<uint64_t> page_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddRequestHeader(request_headers, page_id, header_value_);
}

// static
void DataReductionProxyRequestOptions::AddRequestHeader(
    net::HttpRequestHeaders* request_headers,
    base::Optional<uint64_t> page_id,
    const std::string& session_header_value) {
  DCHECK(!page_id || page_id.value() > 0u);
  std::string header_value;
  if (request_headers->HasHeader(chrome_proxy_header())) {
    request_headers->GetHeader(chrome_proxy_header(), &header_value);
    request_headers->RemoveHeader(chrome_proxy_header());
    header_value += ", ";
  }
  request_headers->SetHeader(chrome_proxy_header(),
                             header_value + session_header_value);
  if (page_id)
    AddPageIDRequestHeader(request_headers, page_id.value());
}

void DataReductionProxyRequestOptions::UpdateCredentials() {
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::SetKeyForTesting(
    const std::string& key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if(!key.empty()) {
    key_ = key;
    UpdateCredentials();
  }
}

void DataReductionProxyRequestOptions::SetSecureSession(
    const std::string& secure_session) {
  DCHECK(thread_checker_.CalledOnValidThread());
  secure_session_ = secure_session;
  // Reset Page ID, so users can't be tracked across sessions.
  ResetPageId();
  RegenerateRequestHeaderValue();
}

void DataReductionProxyRequestOptions::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SetSecureSession(std::string());
}

std::string DataReductionProxyRequestOptions::GetDefaultKey() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string key =
    command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);
// Chrome on iOS gets the default key from a preprocessor constant. Chrome on
// Android and Chrome on desktop get the key from google_apis. Cronet and
// Webview have no default key.
#if defined(OS_IOS)
#if defined(SPDY_PROXY_AUTH_VALUE)
  if (key.empty())
    key = SPDY_PROXY_AUTH_VALUE;
#endif
#elif USE_GOOGLE_API_KEYS_FOR_AUTH_KEY
  if (key.empty()) {
    key = google_apis::GetSpdyProxyAuthValue();
  }
#endif  // defined(OS_IOS)
  return key;
}

const std::string& DataReductionProxyRequestOptions::GetSecureSession() const {
  return secure_session_;
}

void DataReductionProxyRequestOptions::RegenerateRequestHeaderValue() {
  std::vector<std::string> headers;
  if (!secure_session_.empty()) {
    headers.push_back(
        FormatOption(kSecureSessionHeaderOption, secure_session_));
  }
  if (!client_.empty())
    headers.push_back(FormatOption(kClientHeaderOption, client_));

  DCHECK(!build_.empty());
  headers.push_back(FormatOption(kBuildNumberHeaderOption, build_));

  DCHECK(!patch_.empty());
  headers.push_back(FormatOption(kPatchNumberHeaderOption, patch_));

  if (!server_experiments_.empty()) {
    headers.push_back(
        FormatOption(params::GetDataSaverServerExperimentsOptionName(),
                     server_experiments_));
  }

  header_value_ = base::JoinString(headers, ", ");

  if (update_header_callback_) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(chrome_proxy_header(), header_value_);
    update_header_callback_.Run(std::move(headers));
  }
}

// static
base::Optional<std::string>
DataReductionProxyRequestOptions::GetSessionKeyFromRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {
  base::StringPairs kv_pairs;
  if (!ParseChromeProxyHeader(request_headers, &kv_pairs))
    return base::nullopt;

  for (const auto& kv_pair : kv_pairs) {
    // Delete leading and trailing white space characters from the key before
    // comparing.
    if (base::TrimWhitespaceASCII(kv_pair.first, base::TRIM_ALL) ==
        kSecureSessionHeaderOption) {
      return base::TrimWhitespaceASCII(kv_pair.second, base::TRIM_ALL)
          .as_string();
    }
  }
  return base::nullopt;
}

// static
base::Optional<uint64_t>
DataReductionProxyRequestOptions::GetPageIdFromRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {
  base::StringPairs kv_pairs;
  if (!ParseChromeProxyHeader(request_headers, &kv_pairs))
    return base::nullopt;

  for (const auto& kv_pair : kv_pairs) {
    // Delete leading and trailing white space characters from the key before
    // comparing.
    if (base::TrimWhitespaceASCII(kv_pair.first, base::TRIM_ALL) ==
        kPageIdOption) {
      uint64_t page_id;
      if (base::StringToUint64(
              base::TrimWhitespaceASCII(kv_pair.second, base::TRIM_ALL)
                  .as_string(),
              &page_id)) {
        return page_id;
      }

      // Also attempt parsing the page_id as a hex string.
      if (base::HexStringToUInt64(
              base::TrimWhitespaceASCII(kv_pair.second, base::TRIM_ALL)
                  .as_string(),
              &page_id)) {
        return page_id;
      }
    }
  }
  return base::nullopt;
}

uint64_t DataReductionProxyRequestOptions::GeneratePageId() {
  // Caller should not depend on order.
  return ++current_page_id_;
}

void DataReductionProxyRequestOptions::ResetPageId() {
  current_page_id_ = base::RandUint64();
}

}  // namespace data_reduction_proxy
