// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_uri_runtime_class_factory.h"

#include <wrl/implements.h>

#include "base/win/scoped_hstring.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

using ABI::Windows::Foundation::IUriRuntimeClass;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace webshare {

TEST(FakeUriRuntimeClassFactoryTest, CreateUri) {
  auto factory = Make<FakeUriRuntimeClassFactory>();

  auto uri = base::win::ScopedHString::Create("https://www.site.come");
  ComPtr<IUriRuntimeClass> uri_runtime_class;
  ASSERT_HRESULT_SUCCEEDED(factory->CreateUri(uri.get(), &uri_runtime_class));

  HSTRING result;
  ASSERT_HRESULT_SUCCEEDED(uri_runtime_class->get_RawUri(&result));
  auto wrapped_result = base::win::ScopedHString(result);
  ASSERT_EQ(wrapped_result.GetAsUTF8(), "https://www.site.come");
}

TEST(FakeUriRuntimeClassFactoryTest, CreateUri_Invalid) {
  auto factory = Make<FakeUriRuntimeClassFactory>();

  auto uri = base::win::ScopedHString::Create("");
  ComPtr<IUriRuntimeClass> uri_runtime_class;
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(factory->CreateUri(uri.get(), &uri_runtime_class)),
      "CreateUri");

  uri = base::win::ScopedHString::Create(" ");
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(factory->CreateUri(uri.get(), &uri_runtime_class)),
      "CreateUri");

  uri = base::win::ScopedHString::Create("abc");
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(factory->CreateUri(uri.get(), &uri_runtime_class)),
      "CreateUri");

  uri = base::win::ScopedHString::Create("http://?k=v");
  EXPECT_NONFATAL_FAILURE(
      ASSERT_HRESULT_FAILED(factory->CreateUri(uri.get(), &uri_runtime_class)),
      "CreateUri");
}

}  // namespace webshare
