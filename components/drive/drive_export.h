// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_DRIVE_EXPORT_H_
#define COMPONENTS_DRIVE_DRIVE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(COMPONENTS_DRIVE_IMPLEMENTATION)
#define COMPONENTS_DRIVE_EXPORT __declspec(dllexport)
#define COMPONENTS_DRIVE_EXPORT_PRIVATE __declspec(dllexport)
#else
#define COMPONENTS_DRIVE_EXPORT __declspec(dllimport)
#define COMPONENTS_DRIVE_EXPORT_PRIVATE __declspec(dllimport)
#endif  // defined(COMPONENTS_DRIVE_IMPLEMENTATION)

#else  // defined(WIN32)
#define COMPONENTS_DRIVE_EXPORT __attribute__((visibility("default")))
#define COMPONENTS_DRIVE_EXPORT_PRIVATE __attribute__((visibility("default")))
#endif

#else  /// defined(COMPONENT_BUILD)
#define COMPONENTS_DRIVE_EXPORT
#define COMPONENTS_DRIVE_EXPORT_PRIVATE
#endif

#endif  // COMPONENTS_DRIVE_DRIVE_EXPORT_H_
