// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_DEVTOOLS_EXPORT_H_
#define COMPONENTS_UI_DEVTOOLS_DEVTOOLS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(UI_DEVTOOLS_IMPLEMENTATION)
#define UI_DEVTOOLS_EXPORT __declspec(dllexport)
#else
#define UI_DEVTOOLS_EXPORT __declspec(dllimport)
#endif  // defined(UI_DEVTOOLS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(UI_DEVTOOLS_IMPLEMENTATION)
#define UI_DEVTOOLS_EXPORT __attribute__((visibility("default")))
#else
#define UI_DEVTOOLS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define UI_DEVTOOLS_EXPORT
#endif

#endif  // COMPONENTS_UI_DEVTOOLS_DEVTOOLS_EXPORT_H_
