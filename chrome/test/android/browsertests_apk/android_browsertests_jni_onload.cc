// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_utils.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/app/android/chrome_jni_onload.h"
#include "chrome/test/base/chrome_test_launcher.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/nested_message_pump_android.h"
#include "content/public/test/network_service_test_helper.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {
bool NativeInit(base::android::LibraryProcessType) {
  static base::NoDestructor<content::NetworkServiceTestHelper>
      network_service_test_helper;

  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitchValueASCII(switches::kProcessType) ==
          switches::kUtilityProcess &&
      command_line->GetSwitchValueASCII(switches::kUtilitySubType) ==
          network::mojom::NetworkService::Name_) {
    ChromeContentUtilityClient::SetNetworkBinderCreationCallback(base::BindOnce(
        [](content::NetworkServiceTestHelper* helper,
           service_manager::BinderRegistry* registry) {
          helper->RegisterNetworkBinders(registry);
        },
        network_service_test_helper.get()));
  }
  return true;
}
}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  // We avoid calling chrome's android::OnJNIOnLoadInit() so we can inject
  // our own ChromeMainDelegate, and avoid it setting the wrong version number.
  // We jump directly to the content method instead, which chrome's would call
  // also.
  if (!content::android::OnJNIOnLoadInit())
    return -1;

  // This needs to be done before base::TestSuite::Initialize() is called,
  // as it also tries to set MessagePumpForUIFactory.
  base::MessagePump::OverrideMessagePumpForUIFactory(
      []() -> std::unique_ptr<base::MessagePump> {
        return std::make_unique<content::NestedMessagePumpAndroid>();
      });

  content::SetContentMainDelegate(new ChromeTestChromeMainDelegate());
  base::android::SetNativeInitializationHook(NativeInit);

  return JNI_VERSION_1_4;
}
