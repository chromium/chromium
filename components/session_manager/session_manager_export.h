// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_EXPORT_H_
#define COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(SESSION_IMPLEMENTATION)
#define SESSION_EXPORT __declspec(dllexport)
#else
#define SESSION_EXPORT __declspec(dllimport)
#endif  // defined(SESSION_EXPORT)

#else  // defined(WIN32)
#if defined(SESSION_IMPLEMENTATION)
#define SESSION_EXPORT __attribute__((visibility("default")))
#else
#define SESSION_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define SESSION_EXPORT
#endif

#endif  // COMPONENTS_SESSION_MANAGER_SESSION_MANAGER_EXPORT_H_
