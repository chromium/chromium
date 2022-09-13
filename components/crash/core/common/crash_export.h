// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_EXPORT_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CRASH_CORE_COMMON_IMPLEMENTATION)
#define CRASH_EXPORT __declspec(dllexport)
#else
#define CRASH_EXPORT __declspec(dllimport)
#endif  // defined(CRASH_CORE_COMMON_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CRASH_CORE_COMMON_IMPLEMENTATION)
#define CRASH_EXPORT __attribute__((visibility("default")))
#else
#define CRASH_EXPORT
#endif  // defined(CRASH_CORE_COMMON_IMPLEMENTATION)
#endif

#else  // defined(COMPONENT_BUILD)
#define CRASH_EXPORT
#endif

// See BUILD.gn :crash_key target for the declaration.
#if !defined(CRASH_KEY_EXPORT)
#define CRASH_KEY_EXPORT
#endif

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_EXPORT_H_
