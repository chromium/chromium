// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/notification_helper/com_server_module.h"

#include <memory>

#include <wrl/client.h>

#include "base/win/scoped_com_initializer.h"
#include "chrome/install_static/install_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class ComServerModuleTest : public testing::Test {
 public:
  ComServerModuleTest(const ComServerModuleTest&) = delete;
  ComServerModuleTest& operator=(const ComServerModuleTest&) = delete;

 protected:
  ComServerModuleTest() = default;

  void SetUp() override {
    scoped_com_initializer_ =
        std::make_unique<base::win::ScopedCOMInitializer>();
    ASSERT_TRUE(scoped_com_initializer_->Succeeded());

    server_module_ = std::make_unique<notification_helper::ComServerModule>();

    // Since the COM class object is registered in the test, the test executable
    // is now the COM server.
    HRESULT hr = server_module_->RegisterClassObjects();
    if (SUCCEEDED(hr))
      class_registration_succeeded_ = true;

    ASSERT_HRESULT_SUCCEEDED(hr);
  }

  void TearDown() override {
    if (class_registration_succeeded_)
      server_module_->UnregisterClassObjects();

    server_module_.reset();
    scoped_com_initializer_.reset();
  }

  notification_helper::ComServerModule* server_module() {
    return server_module_.get();
  }

 private:
  std::unique_ptr<base::win::ScopedCOMInitializer> scoped_com_initializer_;

  // The server module that holds the COM object class.
  std::unique_ptr<notification_helper::ComServerModule> server_module_;

  // A flag indicating if class registration succeeds.
  bool class_registration_succeeded_ = false;
};

TEST_F(ComServerModuleTest, EventSignalTest) {
  // The waitable event starts unsignaled.
  ASSERT_FALSE(server_module()->IsEventSignaled());

  Microsoft::WRL::ComPtr<IUnknown> notification_activator;
  ASSERT_HRESULT_SUCCEEDED(::CoCreateInstance(
      install_static::GetToastActivatorClsid(), nullptr, CLSCTX_LOCAL_SERVER,
      IID_PPV_ARGS(&notification_activator)));

  // An object instance has been created upon the request, and is hold by the
  // server module. Therefore, the waitable event remains unsignaled.
  ASSERT_FALSE(server_module()->IsEventSignaled());

  // Release the instance object. Now that the last (and the only) instance
  // object of the module is released, the event becomes signaled.
  notification_activator.Reset();
  ASSERT_TRUE(server_module()->IsEventSignaled());
}
