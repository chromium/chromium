// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NOTIFICATION_HELPER_TRACE_UTIL_H_
#define CHROME_NOTIFICATION_HELPER_TRACE_UTIL_H_

#if defined(NDEBUG)
#define Trace(format, ...) ((void)0)
#else
#define Trace(format, ...) TraceImpl(format, ##__VA_ARGS__)
void TraceImpl(const wchar_t* format, ...);
#endif  // defined(NDEBUG)

#endif  // CHROME_NOTIFICATION_HELPER_TRACE_UTIL_H_
