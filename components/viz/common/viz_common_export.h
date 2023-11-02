// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIZ_COMMON_EXPORT_H_
#define COMPONENTS_VIZ_COMMON_VIZ_COMMON_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIZ_COMMON_IMPLEMENTATION)
#define VIZ_COMMON_EXPORT __declspec(dllexport)
#else
#define VIZ_COMMON_EXPORT __declspec(dllimport)
#endif  // defined(VIZ_COMMON_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIZ_COMMON_IMPLEMENTATION)
#define VIZ_COMMON_EXPORT __attribute__((visibility("default")))
#else
#define VIZ_COMMON_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define VIZ_COMMON_EXPORT
#endif

#endif  // COMPONENTS_VIZ_COMMON_VIZ_COMMON_EXPORT_H_
