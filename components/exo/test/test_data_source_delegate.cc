// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_data_source_delegate.h"

#include <vector>

#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace exo::test {

TestDataSourceDelegate::TestDataSourceDelegate() = default;
TestDataSourceDelegate::~TestDataSourceDelegate() = default;

void TestDataSourceDelegate::OnSend(const std::string& mime_type,
                                    base::ScopedFD fd) {
  if (data_map_.empty()) {
    constexpr char kTestData[] = "TestData";
    base::WriteFileDescriptor(fd.get(), kTestData);
  } else {
    auto it = data_map_.find(mime_type);
    base::WriteFileDescriptor(
        fd.get(), it != data_map_.end() ? it->second : std::string());
  }
}

void TestDataSourceDelegate::OnCancelled() {
  cancelled_ = true;
}

bool TestDataSourceDelegate::CanAcceptDataEventsForSurface(
    Surface* surface) const {
  return can_accept_;
}

SecurityDelegate* TestDataSourceDelegate::GetSecurityDelegate() const {
  return security_delegate_.get();
}

void TestDataSourceDelegate::SetData(const std::string& mime_type,
                                     std::string data) {
  data_map_.insert_or_assign(mime_type, std::move(data));
}

}  // namespace exo::test
