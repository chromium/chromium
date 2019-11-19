// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_METAL_UTIL_EXPORT_H_
#define COMPONENTS_METAL_UTIL_METAL_UTIL_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(METAL_UTIL_IMPLEMENTATION)
#define METAL_UTIL_EXPORT __declspec(dllexport)
#else
#define METAL_UTIL_EXPORT __declspec(dllimport)
#endif  // defined(METAL_UTIL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(METAL_UTIL_IMPLEMENTATION)
#define METAL_UTIL_EXPORT __attribute__((visibility("default")))
#else
#define METAL_UTIL_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define METAL_UTIL_EXPORT
#endif

#endif  // COMPONENTS_METAL_UTIL_METAL_UTIL_EXPORT_H_
