// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_EXPORT_H_
#define COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(DOMAIN_RELIABILITY_IMPLEMENTATION)
#define DOMAIN_RELIABILITY_EXPORT __declspec(dllexport)
#else
#define DOMAIN_RELIABILITY_EXPORT __declspec(dllimport)
#endif

#else  // defined(WIN32)

#if defined(DOMAIN_RELIABILITY_IMPLEMENTATION)
#define DOMAIN_RELIABILITY_EXPORT __attribute__((visibility("default")))
#else
#define DOMAIN_RELIABILITY_EXPORT
#endif

#endif  // defined(WIN32)
#else  // defined(COMPONENT_BUILD)

#define DOMAIN_RELIABILITY_EXPORT

#endif

#endif  // COMPONENTS_DOMAIN_RELIABILITY_DOMAIN_RELIABILITY_EXPORT_H_
