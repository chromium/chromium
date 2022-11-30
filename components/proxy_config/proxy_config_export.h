// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_EXPORT_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(PROXY_CONFIG_IMPLEMENTATION)
#define PROXY_CONFIG_EXPORT __declspec(dllexport)
#else
#define PROXY_CONFIG_EXPORT __declspec(dllimport)
#endif  // defined(PROXY_CONFIG_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(PROXY_CONFIG_IMPLEMENTATION)
#define PROXY_CONFIG_EXPORT __attribute__((visibility("default")))
#else
#define PROXY_CONFIG_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define PROXY_CONFIG_EXPORT
#endif

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_EXPORT_H_
