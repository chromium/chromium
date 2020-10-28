// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/utility/content_utility_client.h"
#include "printing/buildflags/buildflags.h"

namespace printing {
class PrintingHandler;
}

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  using NetworkBinderCreationCallback =
      base::OnceCallback<void(service_manager::BinderRegistry*)>;

  ChromeContentUtilityClient();
  ~ChromeContentUtilityClient() override;

  // content::ContentUtilityClient:
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void RegisterNetworkBinders(
      service_manager::BinderRegistry* registry) override;
  void UtilityThreadStarted() override;
  void RegisterMainThreadServices(mojo::ServiceFactory& services) override;
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override;

  // See NetworkBinderProvider above.
  static void SetNetworkBinderCreationCallback(
      NetworkBinderCreationCallback callback);

 private:
#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Last IPC message handler.
  std::unique_ptr<printing::PrintingHandler> printing_handler_;
#endif

  // True if the utility process runs with elevated privileges.
  bool utility_process_running_elevated_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentUtilityClient);
};

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
