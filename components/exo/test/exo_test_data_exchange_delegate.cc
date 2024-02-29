// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_data_exchange_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/pickle.h"
#include "ui/base/clipboard/file_info.h"

namespace exo {

TestDataExchangeDelegate::TestDataExchangeDelegate() = default;

TestDataExchangeDelegate::~TestDataExchangeDelegate() = default;

ui::EndpointType TestDataExchangeDelegate::GetDataTransferEndpointType(
    aura::Window* window) const {
  return endpoint_type_;
}

std::string TestDataExchangeDelegate::GetMimeTypeForUriList(
    ui::EndpointType target) const {
  return "text/uri-list";
}

bool TestDataExchangeDelegate::HasUrlsInPickle(
    const base::Pickle& pickle) const {
  return true;
}

std::vector<ui::FileInfo> TestDataExchangeDelegate::ParseFileSystemSources(
    const ui::DataTransferEndpoint* source,
    const base::Pickle& pickle) const {
  std::vector<ui::FileInfo> file_info;
  return file_info;
}

}  // namespace exo
