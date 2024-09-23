// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
#define CHROMECAST_APP_CAST_MAIN_DELEGATE_H_

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "chromecast/common/cast_content_client.h"
#include "content/public/app/content_main_delegate.h"

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

class CastMainDelegate : public content::ContentMainDelegate {
 public:
  CastMainDelegate();

  CastMainDelegate(const CastMainDelegate&) = delete;
  CastMainDelegate& operator=(const CastMainDelegate&) = delete;

  ~CastMainDelegate() override;

  // content::ContentMainDelegate implementation:
  std::optional<int> BasicStartupComplete() override;
  void PreSandboxStartup() override;
  absl::variant<int, content::MainFunctionParams> RunProcess(
      const std::string& process_type,
      content::MainFunctionParams main_function_params) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void ZygoteForked() override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  bool ShouldCreateFeatureList(InvokedIn invoked_in) override;
  bool ShouldInitializeMojo(InvokedIn invoked_in) override;
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

 private:
  void InitializeResourceBundle();

  std::unique_ptr<CastContentBrowserClient> browser_client_;
  std::unique_ptr<CastContentGpuClient> gpu_client_;
  std::unique_ptr<CastContentRendererClient> renderer_client_;
  std::unique_ptr<CastResourceDelegate> resource_delegate_;
  CastContentClient content_client_;

#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
#endif  // BUILDFLAG(IS_ANDROID)

  std::unique_ptr<CastFeatureListCreator> cast_feature_list_creator_;
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_APP_CAST_MAIN_DELEGATE_H_
