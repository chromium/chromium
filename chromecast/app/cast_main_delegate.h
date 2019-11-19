// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
#define CHROMECAST_APP_CAST_MAIN_DELEGATE_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chromecast/common/cast_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace base {
class FieldTrialList;
}  // namespace base

namespace content {
class BrowserMainRunner;
}  // namespace content

namespace chromecast {

class CastResourceDelegate;
class CastFeatureListCreator;

namespace shell {

class CastContentBrowserClient;
class CastContentGpuClient;
class CastContentRendererClient;
class CastContentUtilityClient;

class CastMainDelegate : public content::ContentMainDelegate {
 public:
  CastMainDelegate();
  ~CastMainDelegate() override;

  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
#if defined(OS_LINUX)
  void ZygoteForked() override;
#endif  // defined(OS_LINUX)
  bool ShouldCreateFeatureList() override;
  void PostEarlyInitialization(bool is_running_tests) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
  content::ContentUtilityClient* CreateContentUtilityClient() override;

 private:
  void InitializeResourceBundle();

  std::unique_ptr<CastContentBrowserClient> browser_client_;
  std::unique_ptr<CastContentGpuClient> gpu_client_;
  std::unique_ptr<CastContentRendererClient> renderer_client_;
  std::unique_ptr<CastContentUtilityClient> utility_client_;
  std::unique_ptr<CastResourceDelegate> resource_delegate_;
  CastContentClient content_client_;

#if defined(OS_ANDROID)
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
#endif  // defined(OS_ANDROID)

  // |field_trial_list_| is a singleton-like that needs to live for as long as
  // anything uses it. It is accessible through |FieldTrialList| static methods,
  // but gives no warning if those methods are called without some instance
  // existing somewhere.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  std::unique_ptr<CastFeatureListCreator> cast_feature_list_creator_;

  DISALLOW_COPY_AND_ASSIGN(CastMainDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
