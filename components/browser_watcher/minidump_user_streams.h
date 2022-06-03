// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_MINIDUMP_USER_STREAMS_H_
#define COMPONENTS_BROWSER_WATCHER_MINIDUMP_USER_STREAMS_H_

namespace browser_watcher {

// The stream type assigned to the minidump stream that holds the serialized
// stability report.
// Note: the value was obtained by adding 1 to the stream type used for holding
// the SyzyAsan proto.
constexpr uint32_t kActivityReportStreamType = 0x4B6B0002;

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_MINIDUMP_USER_STREAMS_H_
