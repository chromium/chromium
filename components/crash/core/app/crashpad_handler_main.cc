// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/debug/debugging_buildflags.h"
#include "base/memory/scoped_refptr.h"
#include "components/gwp_asan/buildflags/buildflags.h"
#include "third_party/crashpad/crashpad/handler/handler_main.h"
#include "third_party/crashpad/crashpad/handler/user_stream_data_source.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#include "components/stability_report/user_stream_data_source_posix.h"
#endif

#if BUILDFLAG(ENABLE_GWP_ASAN)
#include "components/gwp_asan/crash_handler/crash_handler.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#include "components/allocation_recorder/crash_handler/allocation_recorder_holder.h"  // nogncheck
#include "components/allocation_recorder/crash_handler/stream_data_source_factory.h"  // nogncheck
#include "components/allocation_recorder/crash_handler/user_stream_data_source.h"  // nogncheck
#endif

extern "C" {

__attribute__((visibility("default"), used)) int CrashpadHandlerMain(
    int argc,
    char* argv[]) {
  crashpad::UserStreamDataSources user_stream_data_sources;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  user_stream_data_sources.push_back(
      std::make_unique<stability_report::UserStreamDataSourcePosix>());
#endif

#if BUILDFLAG(ENABLE_GWP_ASAN)
  user_stream_data_sources.push_back(
      std::make_unique<gwp_asan::UserStreamDataSource>());
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  user_stream_data_sources.push_back(
      std::make_unique<allocation_recorder::crash_handler::
                           AllocationRecorderStreamDataSource>(
          base::MakeRefCounted<allocation_recorder::crash_handler::
                                   AllocationRecorderHolder>(),
          base::MakeRefCounted<
              allocation_recorder::crash_handler::StreamDataSourceFactory>()));
#endif

  return crashpad::HandlerMain(argc, argv, &user_stream_data_sources);
}

}  // extern "C"
