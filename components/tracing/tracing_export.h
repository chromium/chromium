// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_TRACING_EXPORT_H_
#define COMPONENTS_TRACING_TRACING_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(TRACING_IMPLEMENTATION)
#define TRACING_EXPORT __declspec(dllexport)
#else
#define TRACING_EXPORT __declspec(dllimport)
#endif  // defined(TRACING_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(TRACING_IMPLEMENTATION)
#define TRACING_EXPORT __attribute__((visibility("default")))
#else
#define TRACING_EXPORT
#endif // defined(TRACING_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define TRACING_EXPORT
#endif

#endif  // COMPONENTS_TRACING_TRACING_EXPORT_H_
