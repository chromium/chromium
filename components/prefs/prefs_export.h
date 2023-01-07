// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREFS_EXPORT_H_
#define COMPONENTS_PREFS_PREFS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(COMPONENTS_PREFS_IMPLEMENTATION)
#define COMPONENTS_PREFS_EXPORT __declspec(dllexport)
#else
#define COMPONENTS_PREFS_EXPORT __declspec(dllimport)
#endif  // defined(COMPONENTS_PREFS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(COMPONENTS_PREFS_IMPLEMENTATION)
#define COMPONENTS_PREFS_EXPORT __attribute__((visibility("default")))
#else
#define COMPONENTS_PREFS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define COMPONENTS_PREFS_EXPORT
#endif

#endif  // COMPONENTS_PREFS_PREFS_EXPORT_H_
