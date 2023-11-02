// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Much of this file has been adapted from Chromium (net/http/http_util.cc) and
// Breakpad (common/linux/http_upload.cc).
// See http://www.ietf.org/rfc/rfc2388.txt for a description of the
// multipart/form-data HTTP message type implemented in this file.
#include "chrome/chrome_cleaner/http/internet_helpers.h"

#include <wchar.h>
#include <winhttp.h>  // NOLINT

#include <algorithm>

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace chrome_cleaner {

namespace {

// Returns the index of the closing quote of the string, if any. |start| points
// at the opening quote.
size_t FindStringEnd(const std::wstring& line, size_t start, wchar_t delim) {
  DCHECK_LT(start, line.length());
  DCHECK_EQ(line[start], delim);
  DCHECK((delim == L'"') || (delim == L'\''));

  const wchar_t set[] = {delim, L'\\', L'\0'};
  for (size_t end = line.find_first_of(set, start + 1);
       end != std::wstring::npos; end = line.find_first_of(set, end + 2)) {
    if (line[end] != L'\\')
      return end;
  }
  return line.length();
}

const wchar_t kHttpLws[] = L" \t";

bool IsLWS(wchar_t c) {
  return ::wcschr(kHttpLws, c) != NULL;
}

void TrimLWS(std::wstring::const_iterator* begin,
             std::wstring::const_iterator* end) {
  // Skip leading whitespace.
  while (*begin < *end && IsLWS((*begin)[0]))
    ++(*begin);

  // Skip trailing whitespace.
  while (*begin < *end && IsLWS((*end)[-1]))
    --(*end);
}

}  // namespace

void ParseContentType(const std::wstring& content_type_str,
                      std::wstring* mime_type,
                      std::wstring* charset,
                      bool* had_charset,
                      std::wstring* boundary) {
  const std::wstring::const_iterator begin = content_type_str.begin();

  // Trim leading and trailing whitespace from type. We include '(' in the
  // trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(kHttpLws);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end =
      content_type_str.find_first_of(std::wstring(kHttpLws) + L";(", type_val);
  if (type_end == std::wstring::npos)
    type_end = content_type_str.length();

  size_t charset_val = 0;
  size_t charset_end = 0;
  bool type_has_charset = false;

  // Iterate over parameters.
  size_t param_start = content_type_str.find_first_of(';', type_end);
  if (param_start != std::string::npos) {
    base::StringTokenizerT<std::wstring, std::wstring::const_iterator>
        tokenizer(begin + param_start, content_type_str.end(), L";");
    tokenizer.set_quote_chars(L"\"");
    while (tokenizer.GetNext()) {
      std::wstring::const_iterator equals_sign =
          std::find(tokenizer.token_begin(), tokenizer.token_end(), L'=');
      if (equals_sign == tokenizer.token_end())
        continue;

      std::wstring::const_iterator param_name_begin = tokenizer.token_begin();
      std::wstring::const_iterator param_name_end = equals_sign;
      TrimLWS(&param_name_begin, &param_name_end);

      std::wstring::const_iterator param_value_begin = equals_sign + 1;
      std::wstring::const_iterator param_value_end = tokenizer.token_end();
      DCHECK(param_value_begin <= tokenizer.token_end());
      TrimLWS(&param_value_begin, &param_value_end);

      if (base::EqualsCaseInsensitiveASCII(
              base::MakeWStringPiece(param_name_begin, param_name_end),
              "charset")) {
        charset_val = param_value_begin - begin;
        charset_end = param_value_end - begin;
        type_has_charset = true;
      } else if (base::EqualsCaseInsensitiveASCII(
                     base::MakeWStringPiece(param_name_begin, param_name_end),
                     "boundary")) {
        if (boundary)
          boundary->assign(param_value_begin, param_value_end);
      }
    }
  }

  if (type_has_charset) {
    // Trim leading and trailing whitespace from charset_val. We include '(' in
    // the trailing trim set to catch media-type comments, which are not at all
    // standard, but may occur in rare cases.
    charset_val = content_type_str.find_first_not_of(kHttpLws, charset_val);
    charset_val = std::min(charset_val, charset_end);
    wchar_t first_char = content_type_str[charset_val];
    if (first_char == L'"' || first_char == L'\'') {
      charset_end = FindStringEnd(content_type_str, charset_val, first_char);
      ++charset_val;
      DCHECK(charset_end >= charset_val);
    } else {
      charset_end = std::min(content_type_str.find_first_of(
                                 std::wstring(kHttpLws) + L";(", charset_val),
                             charset_end);
    }
  }

  // If the server sent "*/*", it is meaningless, so do not store it.
  // Also, if type_val is the same as mime_type, then just update the charset
  // However, if charset is empty and mime_type hasn't changed, then don't
  // wipe-out an existing charset. We also want to reject a mime-type if it does
  // not include a slash. Some servers give junk after the charset parameter,
  // which may include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() != 0 && content_type_str != L"*/*" &&
      content_type_str.find_first_of(L'/') != std::wstring::npos) {
    // The common case here is that mime_type is empty.
    bool eq = !mime_type->empty() &&
              base::EqualsCaseInsensitiveASCII(
                  base::MakeWStringPiece(begin + type_val, begin + type_end),
                  base::WideToUTF8(*mime_type).data());
    if (!eq) {
      mime_type->assign(base::ToLowerASCII(
          base::MakeWStringPiece(begin + type_val, begin + type_end)));
    }
    if ((!eq && *had_charset) || type_has_charset) {
      *had_charset = true;
      charset->assign(base::ToLowerASCII(
          base::MakeWStringPiece(begin + charset_val, begin + charset_end)));
    }
  }
}

bool DecomposeUrl(const std::wstring& url,
                  std::wstring* scheme,
                  std::wstring* host,
                  uint16_t* port,
                  std::wstring* path) {
  DCHECK(scheme);
  DCHECK(host);
  DCHECK(path);

  wchar_t scheme_buffer[16], host_buffer[256], path_buffer[256];
  URL_COMPONENTS components;
  memset(&components, 0, sizeof(components));
  components.dwStructSize = sizeof(components);
  components.lpszScheme = scheme_buffer;
  components.dwSchemeLength = sizeof(scheme_buffer) / sizeof(scheme_buffer[0]);
  components.lpszHostName = host_buffer;
  components.dwHostNameLength = sizeof(host_buffer) / sizeof(host_buffer[0]);
  components.lpszUrlPath = path_buffer;
  components.dwUrlPathLength = sizeof(path_buffer) / sizeof(path_buffer[0]);
  if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &components))
    return false;
  *scheme = scheme_buffer;
  *host = host_buffer;
  *path = path_buffer;
  *port = components.nPort;
  return true;
}

std::wstring ComposeUrl(const std::wstring& host,
                        uint16_t port,
                        const std::wstring& path,
                        bool secure) {
  if (secure) {
    if (port == 443)
      return L"https://" + host + path;
    return L"https://" + host + L':' + base::NumberToWString(port) + path;
  }
  if (port == 80)
    return L"http://" + host + path;
  return L"http://" + host + L':' + base::NumberToWString(port) + path;
}

std::wstring GenerateMultipartHttpRequestBoundary() {
  // The boundary has 27 '-' characters followed by 16 hex digits.
  static const wchar_t kBoundaryPrefix[] = L"---------------------------";
  static const size_t kBoundaryLength = 27 + 16;

  // Generate some random numbers to fill out the boundary.
  int r0 = rand();
  int r1 = rand();

  // Add one character for the NULL termination.
  wchar_t temp[kBoundaryLength + 1];
  ::swprintf(temp, sizeof(temp) / sizeof(*temp), L"%s%08X%08X", kBoundaryPrefix,
             r0, r1);

  return std::wstring(temp, kBoundaryLength);
}

std::wstring GenerateMultipartHttpRequestContentTypeHeader(
    const std::wstring boundary) {
  return L"Content-Type: multipart/form-data; boundary=" + boundary;
}

std::string GenerateMultipartHttpRequestBody(
    const std::map<std::wstring, std::wstring>& parameters,
    const std::string& upload_file,
    const std::wstring& file_part_name,
    const std::wstring& boundary) {
  DCHECK(!boundary.empty());
  DCHECK(!file_part_name.empty());
  std::string boundary_utf8 = base::WideToUTF8(boundary);

  std::string request_body;

  // Append each of the parameter pairs as a form-data part.
  for (const auto& entry : parameters) {
    request_body.append("--" + boundary_utf8 + "\r\n");
    request_body.append("Content-Disposition: form-data; name=\"" +
                        base::WideToUTF8(entry.first) + "\"\r\n\r\n" +
                        base::WideToUTF8(entry.second) + "\r\n");
  }

  std::string file_part_name_utf8 = base::WideToUTF8(file_part_name);

  request_body.append("--" + boundary_utf8 + "\r\n");
  request_body.append(
      "Content-Disposition: form-data; "
      "name=\"" +
      file_part_name_utf8 +
      "\"; "
      "filename=\"" +
      file_part_name_utf8 + "\"\r\n");
  request_body.append("Content-Type: application/octet-stream\r\n");
  request_body.append("\r\n");

  request_body.append(upload_file);
  request_body.append("\r\n");
  request_body.append("--" + boundary_utf8 + "--\r\n");

  return request_body;
}

}  // namespace chrome_cleaner
