// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_PNACL_TRANSLATION_RESOURCE_HOST_H_
#define COMPONENTS_NACL_RENDERER_PNACL_TRANSLATION_RESOURCE_HOST_H_

#include <stdint.h>

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ipc/ipc_platform_file.h"
#include "ipc/message_filter.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/private/pp_file_handle.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace nacl {
struct PnaclCacheInfo;
}

// A class to keep track of requests made to the browser for resources that the
// PNaCl translator needs (e.g. descriptors for the translator nexes, temp
// files, and cached translations).

// "Resource" might not be the best name for the various things that pnacl
// needs from the browser since "Resource" is a Pepper thing...
class PnaclTranslationResourceHost : public IPC::MessageFilter {
 public:
  typedef base::OnceCallback<void(int32_t, bool, PP_FileHandle)>
      RequestNexeFdCallback;

  explicit PnaclTranslationResourceHost(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  PnaclTranslationResourceHost(const PnaclTranslationResourceHost&) = delete;
  PnaclTranslationResourceHost& operator=(const PnaclTranslationResourceHost&) =
      delete;

  void RequestNexeFd(PP_Instance instance,
                     const nacl::PnaclCacheInfo& cache_info,
                     RequestNexeFdCallback callback);
  void ReportTranslationFinished(PP_Instance instance, PP_Bool success);

 protected:
  ~PnaclTranslationResourceHost() override;

 private:
  // Maps the instance with an outstanding cache request to the info
  // about that request.
  typedef std::map<PP_Instance, RequestNexeFdCallback> CacheRequestInfoMap;

  // IPC::MessageFilter implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  void SendRequestNexeFd(PP_Instance instance,
                         const nacl::PnaclCacheInfo& cache_info,
                         RequestNexeFdCallback callback);
  void SendReportTranslationFinished(PP_Instance instance,
                                     PP_Bool success);
  void OnNexeTempFileReply(PP_Instance instance,
                           bool is_hit,
                           IPC::PlatformFileForTransit file);
  void CleanupCacheRequests();

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Should be accessed on the io thread.
  raw_ptr<IPC::Sender> sender_;
  CacheRequestInfoMap pending_cache_requests_;
};

#endif  // COMPONENTS_NACL_RENDERER_PNACL_TRANSLATION_RESOURCE_HOST_H_
