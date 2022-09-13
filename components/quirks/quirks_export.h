// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUIRKS_QUIRKS_EXPORT_H_
#define COMPONENTS_QUIRKS_QUIRKS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(QUIRKS_IMPLEMENTATION)
#define QUIRKS_EXPORT __declspec(dllexport)
#else
#define QUIRKS_EXPORT __declspec(dllimport)
#endif  // defined(QUIRKS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(QUIRKS_IMPLEMENTATION)
#define QUIRKS_EXPORT __attribute__((visibility("default")))
#else
#define QUIRKS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define QUIRKS_EXPORT
#endif

#endif  // COMPONENTS_QUIRKS_QUIRKS_EXPORT_H_
