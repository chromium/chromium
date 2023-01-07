// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_MEMORY_EXPORT_H_
#define COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_MEMORY_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DISCARDABLE_MEMORY_IMPLEMENTATION)
#define DISCARDABLE_MEMORY_EXPORT __declspec(dllexport)
#else
#define DISCARDABLE_MEMORY_EXPORT __declspec(dllimport)
#endif  // defined(DISCARDABLE_MEMORY_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(DISCARDABLE_MEMORY_IMPLEMENTATION)
#define DISCARDABLE_MEMORY_EXPORT __attribute__((visibility("default")))
#else
#define DISCARDABLE_MEMORY_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define DISCARDABLE_MEMORY_EXPORT
#endif

#endif  // COMPONENTS_DISCARDABLE_MEMORY_COMMON_DISCARDABLE_MEMORY_EXPORT_H_
