// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/fake_uri_runtime_class_factory.h"

#include <string>
#include <tuple>

#include "base/notreached.h"
#include "base/win/scoped_hstring.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ABI::Windows::Foundation::IUriRuntimeClass;
using ABI::Windows::Foundation::IWwwFormUrlDecoderRuntimeClass;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::WinRtClassicComMix;

namespace webshare {
namespace {

// Provides an implementation of IUriRuntimeClass for use in GTests.
//
// Note that implementations for all the functions except get_RawUri are
// intentionally omitted. Though they are safe APIs, they have many subtle
// differences from the behaviors of a GURL. To prevent inconsistencies and
// unexpected edge cases get_RawUri should be used to construct a GURL and its
// similar functionality leveraged, rather than relying on these functions.
class FakeUriRuntimeClass final
    : public RuntimeClass<RuntimeClassFlags<WinRtClassicComMix>,
                          IUriRuntimeClass> {
 public:
  explicit FakeUriRuntimeClass(std::string raw_uri) : raw_uri_(raw_uri) {}
  FakeUriRuntimeClass(const FakeUriRuntimeClass&) = delete;
  FakeUriRuntimeClass& operator=(const FakeUriRuntimeClass&) = delete;
  ~FakeUriRuntimeClass() final = default;

  // ABI::Windows::Foundation::IUriRuntimeClass:
  IFACEMETHODIMP get_AbsoluteUri(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_DisplayUri(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Domain(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Extension(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Fragment(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Host(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Password(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Path(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Query(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_QueryParsed(
      IWwwFormUrlDecoderRuntimeClass** www_form_url_decoder) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_RawUri(HSTRING* value) final {
    auto copy = base::win::ScopedHString::Create(raw_uri_);
    *value = copy.release();
    return S_OK;
  }
  IFACEMETHODIMP get_SchemeName(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_UserName(HSTRING* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Port(INT32* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Suspicious(boolean* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  Equals(IUriRuntimeClass* uri, boolean* value) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }
  IFACEMETHODIMP
  CombineUri(HSTRING relative_uri, IUriRuntimeClass** instance) final {
    NOTREACHED_IN_MIGRATION()
        << "get_RawUri should be the only function called on an "
           "IUriRuntimeClass - see FakeUriRuntimeClass.";
    return E_NOTIMPL;
  }

 private:
  std::string raw_uri_;
};

}  // namespace

FakeUriRuntimeClassFactory::FakeUriRuntimeClassFactory() = default;
FakeUriRuntimeClassFactory::~FakeUriRuntimeClassFactory() = default;

IFACEMETHODIMP
FakeUriRuntimeClassFactory::CreateUri(HSTRING uri,
                                      IUriRuntimeClass** instance) {
  if (!uri) {
    ADD_FAILURE() << "CreateUri called with null uri.";
    return E_POINTER;
  }

  // ScopedHString takes ownership of the HSTRING provided to it, but taking
  // ownership is not an expected behavior when passing an HSTRING to a system
  // API, so we use a temporary ScopedHString to make a copy we can safely own
  // and release ownership of the original 'back' to the caller.
  base::win::ScopedHString holder(uri);
  auto uri_string = holder.GetAsUTF8();
  std::ignore = holder.release();

  if (uri_string.empty()) {
    ADD_FAILURE() << "CreateUri called with empty uri.";
    return E_POINTER;
  }

  auto url = GURL(uri_string);
  if (!url.is_valid()) {
    ADD_FAILURE() << "CreateUri called with invalid uri.";
    return E_INVALIDARG;
  }

  auto fake_uri_runtime_class = Make<FakeUriRuntimeClass>(uri_string);
  HRESULT hr = fake_uri_runtime_class->QueryInterface(IID_PPV_ARGS(instance));
  if (FAILED(hr)) {
    EXPECT_HRESULT_SUCCEEDED(hr);
    return hr;
  }
  return S_OK;
}

IFACEMETHODIMP FakeUriRuntimeClassFactory::CreateWithRelativeUri(
    HSTRING base_uri,
    HSTRING relative_uri,
    IUriRuntimeClass** instance) {
  NOTREACHED_IN_MIGRATION();
  return E_NOTIMPL;
}

}  // namespace webshare
