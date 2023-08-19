// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools/devtools_pipe/devtools_pipe.h"

#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"

#if BUILDFLAG(IS_WIN)
#include <io.h>
#include <stddef.h>
#include <stdlib.h>
#include "base/win/windows_types.h"
#else
#include <errno.h>
#include <fcntl.h>
#endif

namespace devtools_pipe {

namespace {

#if BUILDFLAG(IS_WIN)
// A class to temporarily override invalid parameters handler called when the
// CRT detects an invalid argument on Windows.
class ScopedInvalidParameterHandlerOverride {
 public:
  ScopedInvalidParameterHandlerOverride()
      : prev_invalid_parameter_handler_(
            _set_thread_local_invalid_parameter_handler(
                InvalidParameterHandler)) {}

  ScopedInvalidParameterHandlerOverride(
      const ScopedInvalidParameterHandlerOverride&) = delete;
  ScopedInvalidParameterHandlerOverride& operator=(
      const ScopedInvalidParameterHandlerOverride&) = delete;

  ~ScopedInvalidParameterHandlerOverride() {
    _set_thread_local_invalid_parameter_handler(
        prev_invalid_parameter_handler_);
  }

 private:
  // A do nothing invalid parameter handler that causes CRT routine to return
  // error to the caller.
  static void InvalidParameterHandler(const wchar_t* expression,
                                      const wchar_t* function,
                                      const wchar_t* file,
                                      unsigned int line,
                                      uintptr_t reserved) {}

  const _invalid_parameter_handler prev_invalid_parameter_handler_;
};

#endif  // #if BUILDFLAG(IS_WIN)

}  // namespace

bool AreFileDescriptorsOpen() {
#if BUILDFLAG(IS_WIN)
  ScopedInvalidParameterHandlerOverride invalid_parameter_handler_override;
  return reinterpret_cast<HANDLE>(_get_osfhandle(
             content::DevToolsAgentHost::kReadFD)) != INVALID_HANDLE_VALUE &&
         reinterpret_cast<HANDLE>(_get_osfhandle(
             content::DevToolsAgentHost::kWriteFD)) != INVALID_HANDLE_VALUE;
#else
  return fcntl(content::DevToolsAgentHost::kReadFD, F_GETFL) != -1 &&
         fcntl(content::DevToolsAgentHost::kWriteFD, F_GETFL) != -1;
#endif
}

}  // namespace devtools_pipe
