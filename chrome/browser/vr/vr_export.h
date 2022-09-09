// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_VR_EXPORT_H_
#define CHROME_BROWSER_VR_VR_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(VR_IMPLEMENTATION)
#define VR_EXPORT __declspec(dllexport)
#else
#define VR_EXPORT __declspec(dllimport)
#endif  // defined(VR_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(VR_IMPLEMENTATION)
#define VR_EXPORT __attribute__((visibility("default")))
#else
#define VR_EXPORT
#endif  // defined(VR_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define VR_EXPORT
#endif

#endif  // CHROME_BROWSER_VR_VR_EXPORT_H_
