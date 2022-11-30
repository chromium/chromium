// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_URL_MATCHER_EXPORT_H_
#define COMPONENTS_URL_MATCHER_URL_MATCHER_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(URL_MATCHER_IMPLEMENTATION)
#define URL_MATCHER_EXPORT __declspec(dllexport)
#else
#define URL_MATCHER_EXPORT __declspec(dllimport)
#endif  // defined(URL_MATCHER_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(URL_MATCHER_IMPLEMENTATION)
#define URL_MATCHER_EXPORT __attribute__((visibility("default")))
#else
#define URL_MATCHER_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define URL_MATCHER_EXPORT
#endif

#endif  // COMPONENTS_URL_MATCHER_URL_MATCHER_EXPORT_H_
