// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/common/content_client.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_NACL)
#include "content/public/common/content_plugin_info.h"
#endif  // BUILDFLAG(ENABLE_NACL)

namespace embedder_support {
class OriginTrialPolicyImpl;
}

class ChromeContentClient : public content::ContentClient {
 public:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // |kNotPresent| is a placeholder plugin location for plugins that are not
  // currently present in this installation of Chrome, but which can be fetched
  // on-demand and therefore should still appear in navigator.plugins.
  static const base::FilePath::CharType kNotPresent[];
#endif

  static const base::FilePath::CharType kPDFExtensionPluginPath[];
  static const base::FilePath::CharType kPDFInternalPluginPath[];

  ChromeContentClient();
  ~ChromeContentClient() override;

  // The methods below are called by child processes to set the function
  // pointers for built-in plugins. We avoid linking these plugins into
  // chrome_common because then on Windows we would ship them twice because of
  // the split DLL.
#if BUILDFLAG(ENABLE_NACL)
  static void SetNaClEntryFunctions(
      content::ContentPluginInfo::GetInterfaceFunc get_interface,
      content::ContentPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::ContentPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

  void SetActiveURL(const GURL& url, std::string top_origin) override;
  void SetGpuInfo(const gpu::GPUInfo& gpu_info) override;
  void AddPlugins(std::vector<content::ContentPluginInfo>* plugins) override;
  std::vector<url::Origin> GetPdfInternalPluginAllowedOrigins() override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::u16string GetLocalizedString(int message_id,
                                    const std::u16string& replacement) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  std::string GetProcessTypeNameInEnglish(int type) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;
#if BUILDFLAG(IS_ANDROID)
  media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
#endif  // BUILDFLAG(IS_ANDROID)
  void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders) override;

 private:
  // Used to lock when |origin_trial_policy_| is initialized.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<embedder_support::OriginTrialPolicyImpl> origin_trial_policy_;
};

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
