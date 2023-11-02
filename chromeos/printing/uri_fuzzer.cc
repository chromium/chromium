// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>

#include "chromeos/printing/uri.h"

namespace {

std::vector<std::string> CreatePath(size_t size, FuzzedDataProvider* data) {
  std::vector<std::string> path(size);
  for (auto& segment : path)
    segment = data->ConsumeRandomLengthString();
  return path;
}

std::vector<std::pair<std::string, std::string>> CreateQuery(
    size_t size,
    FuzzedDataProvider* data) {
  std::vector<std::pair<std::string, std::string>> query(size);
  for (auto& name_value : query) {
    name_value.first = data->ConsumeRandomLengthString();
    name_value.second = data->ConsumeRandomLengthString();
  }
  return query;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Split fuzzer data into 1-byte mode and input string.
  FuzzedDataProvider fuzz_data(data, size);
  const uint8_t mode = fuzz_data.ConsumeIntegral<uint8_t>();
  const int method = mode % 16;
  const size_t param_size = mode / 16;

  // Call one of the parser method (selected by |method|).
  chromeos::Uri uri;
  switch (method) {
    case 0:
    case 1:
      uri = chromeos::Uri(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 2:
      uri.SetScheme(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 3:
      uri.SetPort(fuzz_data.ConsumeIntegral<uint16_t>());
      break;
    case 4:
      uri.SetUserinfo(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 5:
      uri.SetHost(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 6:
      uri.SetPath(CreatePath(param_size, &fuzz_data));
      break;
    case 7:
      uri.SetQuery(CreateQuery(param_size, &fuzz_data));
      break;
    case 8:
      uri.SetFragment(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 9:
      uri.SetUserinfoEncoded(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 10:
      uri.SetHostEncoded(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 11:
      uri.SetPathEncoded(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 12:
      uri.SetPathEncoded(CreatePath(param_size, &fuzz_data));
      break;
    case 13:
      uri.SetQueryEncoded(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    case 14:
      uri.SetQueryEncoded(CreateQuery(param_size, &fuzz_data));
      break;
    case 15:
      uri.SetFragmentEncoded(fuzz_data.ConsumeRemainingBytesAsString());
      break;
    default:
      // it will never happen
      break;
  }

  // Call all Get methods.
  uri.GetLastParsingError();
  uri.GetNormalized(false);
  uri.GetNormalized(true);
  uri.IsASCII();
  uri.GetScheme();
  uri.GetPort();
  uri.GetUserinfo();
  uri.GetHost();
  uri.GetPath();
  uri.GetQuery();
  uri.GetFragment();
  uri.GetUserinfoEncoded();
  uri.GetHostEncoded();
  uri.GetPathEncoded();
  uri.GetPathEncodedAsString();
  uri.GetQueryEncoded();
  uri.GetQueryEncodedAsString();
  uri.GetFragmentEncoded();

  // Exit.
  return 0;
}
