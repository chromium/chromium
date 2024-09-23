// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_launch_prefetch/app_launch_prefetch.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/win/windows_version.h"

namespace {

BASE_FEATURE(kExpandedPrefetchRange,
             "ExpandedPrefetchRange",
             base::FEATURE_ENABLED_BY_DEFAULT);

// /prefetch:# arguments to use when launching various process types. It has
// been observed that when file reads are consistent for 3 process launches with
// the same /prefetch:# argument, the Windows prefetcher starts issuing reads in
// batch at process launch. Because reads depend on the process type, the
// prefetcher wouldn't be able to observe consistent reads if no /prefetch:#
// arguments were used. Note that the browser process has no /prefetch:#
// argument; as such all other processes must have one in order to avoid
// polluting its profile.

// On Windows versions before Win11 21H2 the value must always be in [1, 8];
// otherwise it is treated as 0 by the Windows prefetcher and will interfere
// with the main process launch.

constexpr base::CommandLine::StringViewType kPrefetchArgument1 = L"/prefetch:1";
constexpr base::CommandLine::StringViewType kPrefetchArgument2 = L"/prefetch:2";
constexpr base::CommandLine::StringViewType kPrefetchArgument3 = L"/prefetch:3";
constexpr base::CommandLine::StringViewType kPrefetchArgument4 = L"/prefetch:4";

// /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content embedders
// and are not to be used by content itself, with caveats: we violate this
// rule with kBrowserBackground using bucket 5, while the 7th bucket is used by
// the crashpad fallback handler.
constexpr base::CommandLine::StringViewType kPrefetchArgument5 = L"/prefetch:5";
// constexpr base::CommandLine::StringViewType kPrefetchArgument6 =
// "/prefetch:6";
constexpr base::CommandLine::StringViewType kPrefetchArgument7 = L"/prefetch:7";

// Catch all for Windows versions before Win 11 21H2.
constexpr base::CommandLine::StringViewType kPrefetchArgument8 = L"/prefetch:8";

// On Windows 11 21H2 and later the prefetch range was expanded to be [1,16]

constexpr base::CommandLine::StringViewType kPrefetchArgument9 = L"/prefetch:9";
constexpr base::CommandLine::StringViewType kPrefetchArgument10 =
    L"/prefetch:10";
constexpr base::CommandLine::StringViewType kPrefetchArgument11 =
    L"/prefetch:11";
constexpr base::CommandLine::StringViewType kPrefetchArgument12 =
    L"/prefetch:12";
constexpr base::CommandLine::StringViewType kPrefetchArgument13 =
    L"/prefetch:13";
constexpr base::CommandLine::StringViewType kPrefetchArgument14 =
    L"/prefetch:14";
// constexpr base::CommandLine::StringViewType kPrefetchArgument15 =
// "/prefetch:15";

// Catch all for Windows versions  Win 11 21H2 and later.
constexpr base::CommandLine::StringViewType kPrefetchArgument16 =
    L"/prefetch:16";

}  // namespace

namespace app_launch_prefetch {

base::CommandLine::StringViewType GetPrefetchSwitch(SubprocessType type) {
  using enum SubprocessType;
  if (base::win::GetVersion() >= base::win::Version::WIN11 &&
      base::FeatureList::GetInstance() &&
      base::FeatureList::IsEnabled(kExpandedPrefetchRange)) {
    // These are the prefetch arguments used on Windows versions
    // for Win11 and later. There are fewer processes using the same
    // values and this should lead to better App Launch PreFetch (ALPF)
    // behavior.

    // kPrefetchArgument8 and kPrefetchArgument15 are currently unused.
    switch (type) {
      case kBrowser:
        NOTREACHED();
      case kRenderer:
        return kPrefetchArgument1;
      case kGPU:
        return kPrefetchArgument2;
      case kPpapi:
        return kPrefetchArgument3;
      case kCrashpad:
        return kPrefetchArgument4;
      case kBrowserBackground:
        return kPrefetchArgument5;
      case kCrashpadFallback:
        return kPrefetchArgument7;
      case kExtension:
        return kPrefetchArgument9;
      case kGPUInfo:
        return kPrefetchArgument10;
      case kUtilityNetworkService:
        return kPrefetchArgument11;
      case kUtilityAudio:
        return kPrefetchArgument12;
      case kUtilityStorage:
        return kPrefetchArgument13;
      case kUtilityOther:
        return kPrefetchArgument14;
      case kCatchAll:
        return kPrefetchArgument16;
    }
  } else {
    // These are the prefetch arguments used on Windows versions
    // before Win11 21H2. There are multiple processes using the same values
    // and this leads to less than optimal App Launch PreFetch (ALPF) behavior.

    // /prefetch:5, /prefetch:6 and /prefetch:7 are reserved for content
    // embedders and are not to be used by content itself, with caveats: we
    // violate this rule with kBrowserBackground using bucket 5, while the 7th
    // bucket is used by the crashpad fallback handler.
    switch (type) {
      case kBrowser:
        NOTREACHED();
      case kRenderer:
        return kPrefetchArgument1;
      case kGPU:
        return kPrefetchArgument2;
      case kExtension:
        return kPrefetchArgument2;
      case kPpapi:
        return kPrefetchArgument3;
      case kUtilityNetworkService:
        return kPrefetchArgument3;
      case kCrashpad:
        return kPrefetchArgument4;
      case kBrowserBackground:
        return kPrefetchArgument5;
      case kCrashpadFallback:
        return kPrefetchArgument7;
      case kCatchAll:
        return kPrefetchArgument8;
      case kGPUInfo:
        return kPrefetchArgument8;
      case kUtilityAudio:
        return kPrefetchArgument8;
      case kUtilityStorage:
        return kPrefetchArgument8;
      case kUtilityOther:
        return kPrefetchArgument8;
    }
  }
}

}  // namespace app_launch_prefetch
