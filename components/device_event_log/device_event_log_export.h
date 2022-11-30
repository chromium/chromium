// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_EXPORT_H_
#define COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(DEVICE_EVENT_LOG_IMPLEMENTATION)
#define DEVICE_EVENT_LOG_EXPORT __declspec(dllexport)
#else
#define DEVICE_EVENT_LOG_EXPORT __declspec(dllimport)
#endif  // defined(DEVICE_EVENT_LOG_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(DEVICE_EVENT_LOG_IMPLEMENTATION)
#define DEVICE_EVENT_LOG_EXPORT __attribute__((visibility("default")))
#else
#define DEVICE_EVENT_LOG_EXPORT
#endif  // defined(DEVICE_EVENT_LOG_IMPLEMENTATION)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define DEVICE_EVENT_LOG_EXPORT

#endif

#endif  // COMPONENTS_DEVICE_EVENT_LOG_DEVICE_EVENT_LOG_EXPORT_H_
