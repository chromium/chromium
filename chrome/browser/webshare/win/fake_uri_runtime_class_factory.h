// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_URI_RUNTIME_CLASS_FACTORY_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_URI_RUNTIME_CLASS_FACTORY_H_

#include <windows.foundation.h>
#include <wrl/implements.h>

namespace webshare {

// Provides an implementation of IUriRuntimeClassFactory for use in GTests.
class __declspec(uuid("93741C10-A511-410F-B2CA-3F0A2B674ECE"))
    FakeUriRuntimeClassFactory final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Foundation::IUriRuntimeClassFactory> {
 public:
  FakeUriRuntimeClassFactory();
  FakeUriRuntimeClassFactory(const FakeUriRuntimeClassFactory&) = delete;
  FakeUriRuntimeClassFactory& operator=(const FakeUriRuntimeClassFactory&) =
      delete;
  ~FakeUriRuntimeClassFactory() final;

  // ABI::Windows::Foundation::IUriRuntimeClassFactory:
  IFACEMETHODIMP
  CreateUri(HSTRING uri,
            ABI::Windows::Foundation::IUriRuntimeClass** instance) final;
  IFACEMETHODIMP CreateWithRelativeUri(
      HSTRING base_uri,
      HSTRING relative_uri,
      ABI::Windows::Foundation::IUriRuntimeClass** instance) final;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_URI_RUNTIME_CLASS_FACTORY_H_
