// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/utility/content_utility_client.h"

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  ChromeContentUtilityClient();

  ChromeContentUtilityClient(const ChromeContentUtilityClient&) = delete;
  ChromeContentUtilityClient& operator=(const ChromeContentUtilityClient&) =
      delete;

  ~ChromeContentUtilityClient() override;

  // content::ContentUtilityClient:
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) override;
  void UtilityThreadStarted() override;
  void RegisterMainThreadServices(mojo::ServiceFactory& services) override;
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::GenericPendingReceiver InitMojoServiceManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  // True if the utility process runs with elevated privileges.
  bool utility_process_running_elevated_ = false;
};

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
