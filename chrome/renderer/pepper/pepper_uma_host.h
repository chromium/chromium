// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PEPPER_UMA_HOST_H_
#define CHROME_RENDERER_PEPPER_PEPPER_UMA_HOST_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/resource_host.h"
#include "url/gurl.h"

namespace content {
class RendererPpapiHost;
}

namespace ppapi {
namespace host {
struct HostMessageContext;
}  // namespace host
}  // namespace ppapi

class PepperUMAHost : public ppapi::host::ResourceHost {
 public:
  PepperUMAHost(content::RendererPpapiHost* host,
                PP_Instance instance,
                PP_Resource resource);

  PepperUMAHost(const PepperUMAHost&) = delete;
  PepperUMAHost& operator=(const PepperUMAHost&) = delete;

  ~PepperUMAHost() override;

  // ppapi::host::ResourceMessageHandler implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

 private:
  bool IsPluginAllowed();
  bool IsHistogramAllowed(const std::string& histogram);

  int32_t OnHistogramCustomTimes(ppapi::host::HostMessageContext* context,
                                 const std::string& name,
                                 int64_t sample,
                                 int64_t min,
                                 int64_t max,
                                 uint32_t bucket_count);

  int32_t OnHistogramCustomCounts(ppapi::host::HostMessageContext* context,
                                  const std::string& name,
                                  int32_t sample,
                                  int32_t min,
                                  int32_t max,
                                  uint32_t bucket_count);

  int32_t OnHistogramEnumeration(ppapi::host::HostMessageContext* context,
                                 const std::string& name,
                                 int32_t sample,
                                 int32_t boundary_value);

  int32_t OnIsCrashReportingEnabled(ppapi::host::HostMessageContext* context);

  const GURL document_url_;
  bool is_plugin_in_process_;
  base::FilePath plugin_base_name_;

  // Set of origins that can use UMA private APIs from NaCl.
  std::set<std::string> allowed_origins_;
  // Set of hashed histogram prefixes that can be used from this interface.
  std::set<std::string> allowed_histogram_prefixes_;
  // Set of plugin files names that are allowed to use this interface.
  std::set<base::FilePath::StringType> allowed_plugin_base_names_;
};

#endif  // CHROME_RENDERER_PEPPER_PEPPER_UMA_HOST_H_
