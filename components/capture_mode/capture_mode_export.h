// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_EXPORT_H_
#define COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CAPTURE_MODE_IMPLEMENTATION)
#define CAPTURE_MODE_EXPORT __declspec(dllexport)
#else
#define CAPTURE_MODE_EXPORT __declspec(dllimport)
#endif  // defined(CAPTURE_MODE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CAPTURE_MODE_IMPLEMENTATION)
#define CAPTURE_MODE_EXPORT __attribute__((visibility("default")))
#else
#define CAPTURE_MODE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CAPTURE_MODE_EXPORT
#endif

#endif  // COMPONENTS_CAPTURE_MODE_CAPTURE_MODE_EXPORT_H_
