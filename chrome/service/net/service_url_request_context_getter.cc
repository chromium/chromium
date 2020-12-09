// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_url_request_context_getter.h"

#include <stdint.h>
#include <utility>

#include "base/compiler_specific.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/service/service_process.h"
#include "components/version_info/version_info.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "net/url_request/url_request_context_builder.h"

#if defined(OS_POSIX) && !defined(OS_MAC)
#include <sys/utsname.h>
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#error "Not supported on ChromeOS"
#endif

namespace {
// Copied from webkit/glue/user_agent.cc. We don't want to pull in a dependency
// on webkit/glue which also pulls in the renderer. Also our user-agent is
// totally different from the user-agent used by the browser, just the
// OS-specific parts are common.
std::string BuildOSCpuInfo() {
  std::string os_cpu;

#if defined(OS_WIN) || defined(OS_MAC)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
#endif
#if defined(OS_POSIX) && !defined(OS_MAC)
  // Should work on any Posix system.
  struct utsname unixinfo;
  uname(&unixinfo);

  std::string cputype;
  // special case for biarch systems
  if (strcmp(unixinfo.machine, "x86_64") == 0 &&
      sizeof(void*) == sizeof(int32_t)) {  // NOLINT
    cputype.assign("i686 (x86_64)");
  } else {
    cputype.assign(unixinfo.machine);
  }
#endif

  base::StringAppendF(
      &os_cpu,
#if defined(OS_WIN)
      "Windows NT %d.%d",
      os_major_version,
      os_minor_version
#elif defined(OS_MAC)
      "Intel Mac OS X %d_%d_%d",
      os_major_version,
      os_minor_version,
      os_bugfix_version
#else
      "%s %s",
      unixinfo.sysname,  // e.g. Linux
      cputype.c_str()    // e.g. i686
#endif
  );  // NOLINT

  return os_cpu;
}

// Returns the default user agent.
std::string MakeUserAgentForServiceProcess() {
  std::string user_agent;
  std::string extra_version_info;
  if (!version_info::IsOfficialBuild())
    extra_version_info = "-devel";
  base::StringAppendF(&user_agent,
                      "Chrome Service %s(%s)%s %s ",
                      version_info::GetVersionNumber().c_str(),
                      version_info::GetLastChange().c_str(),
                      extra_version_info.c_str(),
                      BuildOSCpuInfo().c_str());
  return user_agent;
}

}  // namespace

ServiceURLRequestContextGetter::ServiceURLRequestContextGetter()
    : user_agent_(MakeUserAgentForServiceProcess()),
      network_task_runner_(g_service_process->io_task_runner()) {
  DCHECK(g_service_process);
  proxy_config_service_ =
      net::ConfiguredProxyResolutionService::CreateSystemProxyConfigService(
          g_service_process->io_task_runner());
}

net::URLRequestContext*
ServiceURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_.get()) {
    net::URLRequestContextBuilder builder;
    builder.set_user_agent(user_agent_);
    builder.set_accept_language("en-us,fr");
    builder.set_proxy_config_service(std::move(proxy_config_service_));
    builder.set_throttling_enabled(true);
    url_request_context_ = builder.Build();
  }
  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
ServiceURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

ServiceURLRequestContextGetter::~ServiceURLRequestContextGetter() {}
