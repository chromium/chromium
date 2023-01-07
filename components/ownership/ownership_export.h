// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_OWNERSHIP_EXPORT_H_
#define COMPONENTS_OWNERSHIP_OWNERSHIP_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(OWNERSHIP_IMPLEMENTATION)
#define OWNERSHIP_EXPORT __declspec(dllexport)
#else
#define OWNERSHIP_EXPORT __declspec(dllimport)
#endif  // defined(OWNERSHIP_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(OWNERSHIP_IMPLEMENTATION)
#define OWNERSHIP_EXPORT __attribute__((visibility("default")))
#else
#define OWNERSHIP_EXPORT
#endif  // defined(OWNERSHIP_IMPLEMENTATION)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define OWNERSHIP_EXPORT

#endif

#endif  // COMPONENTS_OWNERSHIP_OWNERSHIP_EXPORT_H_
