// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_COMMON_GRAPHICS_MEMORY_DUMP_PROVIDER_ANDROID_H_
#define COMPONENTS_TRACING_COMMON_GRAPHICS_MEMORY_DUMP_PROVIDER_ANDROID_H_

#include <stddef.h>

#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// Dump provider which collects memory stats about graphics memory on Android.
// This requires the presence of the memtrack_helper daemon, which must be
// executed separetely via a (root) adb shell command. The dump provider will
// fail (and hence disabled by the MemoryDumpManager) in absence of the helper.
// See the design-doc https://goo.gl/4Y30p9 for more details.
class TRACING_EXPORT GraphicsMemoryDumpProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  static GraphicsMemoryDumpProvider* GetInstance();

  GraphicsMemoryDumpProvider(const GraphicsMemoryDumpProvider&) = delete;
  GraphicsMemoryDumpProvider& operator=(const GraphicsMemoryDumpProvider&) =
      delete;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(GraphicsMemoryDumpProviderTest, ParseResponse);
  friend struct base::DefaultSingletonTraits<GraphicsMemoryDumpProvider>;

  void ParseResponseAndAddToDump(const char* buf,
                                 size_t length,
                                 base::trace_event::ProcessMemoryDump* pmd);

  GraphicsMemoryDumpProvider();
  ~GraphicsMemoryDumpProvider() override;

  static const char kDumpBaseName[];  // Used by the unittest.

  // Stores key names coming from the memtrack helper in long-lived storage.
  // This is to allow using cheap char* strings in tracing without copies.
  std::unordered_set<std::string> key_names_;
};

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_GRAPHICS_MEMORY_DUMP_PROVIDER_ANDROID_H_
