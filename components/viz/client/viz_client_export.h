// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_VIZ_CLIENT_EXPORT_H_
#define COMPONENTS_VIZ_CLIENT_VIZ_CLIENT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VIZ_CLIENT_IMPLEMENTATION)
#define VIZ_CLIENT_EXPORT __declspec(dllexport)
#else
#define VIZ_CLIENT_EXPORT __declspec(dllimport)
#endif  // defined(VIZ_CLIENT_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VIZ_CLIENT_IMPLEMENTATION)
#define VIZ_CLIENT_EXPORT __attribute__((visibility("default")))
#else
#define VIZ_CLIENT_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define VIZ_CLIENT_EXPORT
#endif

#endif  // COMPONENTS_VIZ_CLIENT_VIZ_CLIENT_EXPORT_H_
