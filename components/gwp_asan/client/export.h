// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_EXPORT_H_
#define COMPONENTS_GWP_ASAN_CLIENT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(GWP_ASAN_IMPLEMENTATION)
#define GWP_ASAN_EXPORT __declspec(dllexport)
#else
#define GWP_ASAN_EXPORT __declspec(dllimport)
#endif  // defined(GWP_ASAN_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(GWP_ASAN_IMPLEMENTATION)
#define GWP_ASAN_EXPORT __attribute__((visibility("default")))
#else
#define GWP_ASAN_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define GWP_ASAN_EXPORT
#endif

#endif  // COMPONENTS_GWP_ASAN_CLIENT_EXPORT_H_
