// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_EXPORT_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(KEYED_SERVICE_IMPLEMENTATION)
#define KEYED_SERVICE_EXPORT __declspec(dllexport)
#else
#define KEYED_SERVICE_EXPORT __declspec(dllimport)
#endif  // defined(KEYED_SERVICE_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(KEYED_SERVICE_IMPLEMENTATION)
#define KEYED_SERVICE_EXPORT __attribute__((visibility("default")))
#else
#define KEYED_SERVICE_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define KEYED_SERVICE_EXPORT
#endif

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_EXPORT_H_
