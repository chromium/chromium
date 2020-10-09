// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/scoped_fake_data_transfer_manager_interop.h"

#include <windows.applicationmodel.datatransfer.h>
#include <wrl/implements.h>

#include "base/strings/string_piece.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/win_util.h"
#include "chrome/browser/webshare/win/fake_data_transfer_manager_interop.h"
#include "chrome/browser/webshare/win/show_share_ui_for_window_operation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {
namespace {

static FakeDataTransferManagerInterop* g_current_fake_interop = nullptr;

static HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                          const IID& iid,
                                          void** out_factory) {
  base::win::ScopedHString class_id_hstring(class_id);
  EXPECT_STREQ(
      class_id_hstring.Get().data(),
      RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager);
  if (g_current_fake_interop == nullptr) {
    ADD_FAILURE();
    return E_UNEXPECTED;
  }
  *out_factory = g_current_fake_interop;
  g_current_fake_interop->AddRef();
  return S_OK;
}

}  // namespace

ScopedFakeDataTransferManagerInterop::ScopedFakeDataTransferManagerInterop() {
  // Initialization work is done in an independent function so that the
  // various test macros can be used.
  Initialize();
}

ScopedFakeDataTransferManagerInterop::~ScopedFakeDataTransferManagerInterop() {
  g_current_fake_interop = nullptr;
  ShowShareUIForWindowOperation::SetRoGetActivationFactoryFunctionForTesting(
      &base::win::RoGetActivationFactory);
}

FakeDataTransferManagerInterop&
ScopedFakeDataTransferManagerInterop::instance() {
  return *(instance_.Get());
}

void ScopedFakeDataTransferManagerInterop::Initialize() {
  ASSERT_TRUE(base::win::ResolveCoreWinRTDelayload());
  ASSERT_TRUE(base::win::ScopedHString::ResolveCoreWinRTStringDelayload());
  base::win::AssertComInitialized();

  instance_ = Microsoft::WRL::Make<FakeDataTransferManagerInterop>();

  // Confirm there is no competing instance and set this instance
  // as the factory for the data_transfer_manager_util
  ASSERT_EQ(g_current_fake_interop, nullptr);
  g_current_fake_interop = instance_.Get();
  ShowShareUIForWindowOperation::SetRoGetActivationFactoryFunctionForTesting(
      &FakeRoGetActivationFactory);
}

}  // namespace webshare
