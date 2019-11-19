// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/origin_trials/chrome_origin_trial_policy.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/common/content_client.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/common/pepper_plugin_info.h"
#endif

class ChromeContentClient : public content::ContentClient {
 public:
#if defined(GOOGLE_CHROME_BUILD)
  // |kNotPresent| is a placeholder plugin location for plugins that are not
  // currently present in this installation of Chrome, but which can be fetched
  // on-demand and therefore should still appear in navigator.plugins.
  static const base::FilePath::CharType kNotPresent[];
#endif

#if BUILDFLAG(ENABLE_NACL)
  static const base::FilePath::CharType kNaClPluginFileName[];
#endif

  static const char kPDFExtensionPluginName[];
  static const char kPDFInternalPluginName[];
  static const base::FilePath::CharType kPDFPluginPath[];

  ChromeContentClient();
  ~ChromeContentClient() override;

  // The methods below are called by child processes to set the function
  // pointers for built-in plugins. We avoid linking these plugins into
  // chrome_common because then on Windows we would ship them twice because of
  // the split DLL.
#if BUILDFLAG(ENABLE_NACL)
  static void SetNaClEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

#if BUILDFLAG(ENABLE_PLUGINS) && BUILDFLAG(ENABLE_PDF)
  static void SetPDFEntryFunctions(
      content::PepperPluginInfo::GetInterfaceFunc get_interface,
      content::PepperPluginInfo::PPP_InitializeModuleFunc initialize_module,
      content::PepperPluginInfo::PPP_ShutdownModuleFunc shutdown_module);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // This returns the most recent plugin based on the plugin versions. In the
  // event of a tie, a debug plugin will be considered more recent than a
  // non-debug plugin.
  // It does not make sense to call this on a vector that contains more than one
  // plugin type. This function may return a nullptr if given an empty vector.
  // The method is only visible for testing purposes.
  static content::PepperPluginInfo* FindMostRecentPlugin(
      const std::vector<std::unique_ptr<content::PepperPluginInfo>>& plugins);
#endif

  void SetActiveURL(const GURL& url, std::string top_origin) override;
  void SetGpuInfo(const gpu::GPUInfo& gpu_info) override;
  void AddPepperPlugins(
      std::vector<content::PepperPluginInfo>* plugins) override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;

  void AddAdditionalSchemes(Schemes* schemes) override;
  base::string16 GetLocalizedString(int message_id) override;
  base::string16 GetLocalizedString(int message_id,
                                    const base::string16& replacement) override;
  base::StringPiece GetDataResource(int resource_id,
                                    ui::ScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  gfx::Image& GetNativeImageNamed(int resource_id) override;
  base::DictionaryValue GetNetLogConstants() override;
  std::string GetProcessTypeNameInEnglish(int type) override;

  bool AllowScriptExtensionForServiceWorker(
      const url::Origin& script_origin) override;

  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;

#if defined(OS_ANDROID)
  media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
#endif  // OS_ANDROID

  void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders) override;

 private:
  // Used to lock when |origin_trial_policy_| is initialized.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<ChromeOriginTrialPolicy> origin_trial_policy_;
};

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
