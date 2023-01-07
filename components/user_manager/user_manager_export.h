// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_MANAGER_EXPORT_H_
#define COMPONENTS_USER_MANAGER_USER_MANAGER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(USER_MANAGER_IMPLEMENTATION)
#define USER_MANAGER_EXPORT __declspec(dllexport)
#else
#define USER_MANAGER_EXPORT __declspec(dllimport)
#endif  // defined(USER_MANAGER_EXPORT)

#else  // defined(WIN32)
#if defined(USER_MANAGER_IMPLEMENTATION)
#define USER_MANAGER_EXPORT __attribute__((visibility("default")))
#else
#define USER_MANAGER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define USER_MANAGER_EXPORT
#endif

#endif  // COMPONENTS_USER_MANAGER_USER_MANAGER_EXPORT_H_
