// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_EXPORT_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(COMPONENTS_DOWNLOAD_IMPLEMENTATION)
#define COMPONENTS_DOWNLOAD_EXPORT __declspec(dllexport)
#else
#define COMPONENTS_DOWNLOAD_EXPORT __declspec(dllimport)
#endif  // defined(COMPONENTS_DOWNLOAD_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(COMPONENTS_DOWNLOAD_IMPLEMENTATION)
#define COMPONENTS_DOWNLOAD_EXPORT __attribute__((visibility("default")))
#else
#define COMPONENTS_DOWNLOAD_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define COMPONENTS_DOWNLOAD_EXPORT
#endif

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_COMMON_DOWNLOAD_EXPORT_H_
