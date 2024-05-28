// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/win/scoped_share_operation_fake_components.h"

#include <windows.storage.streams.h>
#include <wrl/implements.h>

#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/webshare/win/fake_data_writer_factory.h"
#include "chrome/browser/webshare/win/fake_storage_file_statics.h"
#include "chrome/browser/webshare/win/fake_uri_runtime_class_factory.h"
#include "chrome/browser/webshare/win/scoped_fake_data_transfer_manager_interop.h"
#include "chrome/browser/webshare/win/share_operation.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {
namespace {

static FakeDataWriterFactory* g_current_fake_data_writer_factory = nullptr;
static FakeStorageFileStatics* g_current_fake_storage_file_statics = nullptr;
static FakeUriRuntimeClassFactory* g_current_fake_uri_runtime_class_factory =
    nullptr;

static HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                          const IID& iid,
                                          void** out_factory) {
  void* instance = nullptr;
  base::win::ScopedHString class_id_hstring(class_id);
  if (class_id_hstring.Get() == RuntimeClass_Windows_Storage_StorageFile) {
    instance = g_current_fake_storage_file_statics;
  } else if (class_id_hstring.Get() ==
             RuntimeClass_Windows_Storage_Streams_DataWriter) {
    instance = g_current_fake_data_writer_factory;
  } else if (class_id_hstring.Get() == RuntimeClass_Windows_Foundation_Uri) {
    instance = g_current_fake_uri_runtime_class_factory;
  }

  if (!instance) {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }

  *out_factory = instance;
  reinterpret_cast<IUnknown*>(instance)->AddRef();
  return S_OK;
}

}  // namespace

ScopedShareOperationFakeComponents::ScopedShareOperationFakeComponents() =
    default;

ScopedShareOperationFakeComponents::~ScopedShareOperationFakeComponents() {
  g_current_fake_data_writer_factory = nullptr;
  g_current_fake_storage_file_statics = nullptr;
  g_current_fake_uri_runtime_class_factory = nullptr;
  ShareOperation::SetRoGetActivationFactoryFunctionForTesting(
      &base::win::RoGetActivationFactory);
}

void ScopedShareOperationFakeComponents::SetUp() {
  base::win::AssertComInitialized();

  ASSERT_NO_FATAL_FAILURE(scoped_fake_data_transfer_manager_interop_.SetUp());

  fake_data_writer_factory_ = Microsoft::WRL::Make<FakeDataWriterFactory>();
  fake_storage_file_statics_ = Microsoft::WRL::Make<FakeStorageFileStatics>();
  fake_uri_runtime_class_factory_ =
      Microsoft::WRL::Make<FakeUriRuntimeClassFactory>();

  // Confirm there are no competing instances and set these instances
  // for use by the main factory function
  ASSERT_EQ(g_current_fake_data_writer_factory, nullptr);
  g_current_fake_data_writer_factory = fake_data_writer_factory_.Get();
  ASSERT_EQ(g_current_fake_storage_file_statics, nullptr);
  g_current_fake_storage_file_statics = fake_storage_file_statics_.Get();
  ASSERT_EQ(g_current_fake_uri_runtime_class_factory, nullptr);
  g_current_fake_uri_runtime_class_factory =
      fake_uri_runtime_class_factory_.Get();
  ShareOperation::SetRoGetActivationFactoryFunctionForTesting(
      &FakeRoGetActivationFactory);
}

FakeDataTransferManagerInterop&
ScopedShareOperationFakeComponents::fake_data_transfer_manager_interop() {
  return scoped_fake_data_transfer_manager_interop_.instance();
}

}  // namespace webshare
