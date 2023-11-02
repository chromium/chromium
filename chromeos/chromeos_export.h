// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CHROMEOS_EXPORT_H_
#define CHROMEOS_CHROMEOS_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CHROMEOS_IMPLEMENTATION)
#define CHROMEOS_EXPORT __declspec(dllexport)
#else
#define CHROMEOS_EXPORT __declspec(dllimport)
#endif  // defined(CHROMEOS_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CHROMEOS_IMPLEMENTATION) || defined(IS_CHROMEOS_SYSTEM_IMPL)
#define CHROMEOS_EXPORT __attribute__((visibility("default")))
#else
#define CHROMEOS_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CHROMEOS_EXPORT
#endif

#endif  // CHROMEOS_CHROMEOS_EXPORT_H_
