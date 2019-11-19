// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

#include <Windows.h>
#include <winhttp.h>

#include <atlconv.h>

#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

// static
WinHttpUrlFetcher::CreatorCallback*
WinHttpUrlFetcher::GetCreatorFunctionStorage() {
  static CreatorCallback creator_for_testing;
  return &creator_for_testing;
}

// static
std::unique_ptr<WinHttpUrlFetcher> WinHttpUrlFetcher::Create(const GURL& url) {
  return !GetCreatorFunctionStorage()->is_null()
             ? GetCreatorFunctionStorage()->Run(url)
             : std::unique_ptr<WinHttpUrlFetcher>(new WinHttpUrlFetcher(url));
}

WinHttpUrlFetcher::WinHttpUrlFetcher(const GURL& url)
    : url_(url), session_(nullptr), request_(nullptr) {
  LOGFN(INFO) << "url=" << url.spec() << " (scheme and port ignored)";

  ScopedWinHttpHandle::Handle session = ::WinHttpOpen(
      L"GaiaCP/1.0 (Windows NT)", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "WinHttpOpen hr=" << putHR(hr);
  }
  session_.Set(session);
}

WinHttpUrlFetcher::WinHttpUrlFetcher() {}

WinHttpUrlFetcher::~WinHttpUrlFetcher() {
  // Closing the session handle closes all derived handles too.
}

bool WinHttpUrlFetcher::IsValid() const {
  return session_.IsValid();
}

HRESULT WinHttpUrlFetcher::SetRequestHeader(const char* name,
                                            const char* value) {
  DCHECK(name);
  DCHECK(value);

  // TODO(rogerta): does not support multivalued headers.
  request_headers_[name] = value;
  return S_OK;
}

HRESULT WinHttpUrlFetcher::SetRequestBody(const char* body) {
  DCHECK(body);
  body_ = body;
  return S_OK;
}

HRESULT WinHttpUrlFetcher::SetHttpRequestTimeout(const int timeout_in_millis) {
  DCHECK(timeout_in_millis);
  timeout_in_millis_ = timeout_in_millis;
  return S_OK;
}

HRESULT WinHttpUrlFetcher::Fetch(std::vector<char>* response) {
  USES_CONVERSION;
  DCHECK(response);

  response->clear();

  if (!session_.IsValid()) {
    LOGFN(ERROR) << "Invalid fetcher";
    return E_UNEXPECTED;
  }

  // Open a connection to the server.
  ScopedWinHttpHandle connect;
  {
    ScopedWinHttpHandle::Handle connect_tmp = ::WinHttpConnect(
        session_.Get(), A2CW(url_.host().c_str()), INTERNET_DEFAULT_PORT, 0);
    if (!connect_tmp) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WinHttpConnect hr=" << putHR(hr);
      return hr;
    }
    connect.Set(connect_tmp);
  }

  {
    // Set timeout if specified.
    if (timeout_in_millis_ != 0) {
      if (!::WinHttpSetTimeouts(session_.Get(), timeout_in_millis_,
                                timeout_in_millis_, timeout_in_millis_,
                                timeout_in_millis_)) {
        HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
        LOGFN(ERROR) << "WinHttpSetTimeouts hr=" << putHR(hr);
        return hr;
      }
    }
  }

  {
    bool use_post = !body_.empty();
    ScopedWinHttpHandle::Handle request = ::WinHttpOpenRequest(
        connect.Get(), use_post ? L"POST" : L"GET",
        use_post ? A2CW(url_.path().c_str())
                 : A2CW(url_.PathForRequest().c_str()),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_REFRESH | WINHTTP_FLAG_SECURE);
    if (!request) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WinHttpOpenRequest hr=" << putHR(hr);
      return hr;
    }
    request_.Set(request);
  }

  // Add request headers.

  for (const auto& kv : request_headers_) {
    const wchar_t* key = A2CW(kv.first.c_str());
    const wchar_t* value = A2CW(kv.second.c_str());
    base::string16 header = base::StringPrintf(L"%ls: %ls", key, value);
    if (!::WinHttpAddRequestHeaders(
            request_.Get(), header.c_str(), header.length(),
            WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WinHttpAddRequestHeaders name=" << kv.first
                   << " hr=" << putHR(hr);
      return hr;
    }
  }

  // Write request body if needed.

  if (!::WinHttpSendRequest(request_.Get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            const_cast<char*>(body_.c_str()), body_.length(),
                            body_.length(),
                            reinterpret_cast<DWORD_PTR>(nullptr))) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "WinHttpSendRequest hr=" << putHR(hr);
    return hr;
  }

  // Wait for the response.

  if (!::WinHttpReceiveResponse(request_.Get(), nullptr)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "WinHttpReceiveResponse hr=" << putHR(hr);
    return hr;
  }

  DWORD length = 0;
  if (!::WinHttpQueryDataAvailable(request_.Get(), &length)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    LOGFN(ERROR) << "WinHttpQueryDataAvailable hr=" << putHR(hr);
    return hr;
  }

  // 256k max response size to make sure bad data does not crash GCPW.
  // This fetcher is only used to retrieve small information such as token
  // handle status and profile picture images so it should not need a larger
  // buffer than 256k.
  constexpr size_t kMaxResponseSize = 256 * 1024 * 1024;
  // Read the response.
  std::unique_ptr<char> buffer(new char[length]);
  DWORD actual = 0;
  do {
    if (!::WinHttpReadData(request_.Get(), buffer.get(), length, &actual)) {
      HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
      LOGFN(ERROR) << "WinHttpReadData hr=" << putHR(hr);
      return hr;
    }

    size_t current_size = response->size();
    response->resize(response->size() + actual);
    memcpy(response->data() + current_size, buffer.get(), actual);
    if (response->size() >= kMaxResponseSize) {
      LOGFN(ERROR) << "Response has exceeded max size=" << kMaxResponseSize;
      return E_OUTOFMEMORY;
    }
  } while (actual);

  return S_OK;
}

HRESULT WinHttpUrlFetcher::Close() {
  request_.Close();
  return S_OK;
}

}  // namespace credential_provider
