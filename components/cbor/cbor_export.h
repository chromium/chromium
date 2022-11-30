// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CBOR_CBOR_EXPORT_H_
#define COMPONENTS_CBOR_CBOR_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(CBOR_IMPLEMENTATION)
#define CBOR_EXPORT __declspec(dllexport)
#else
#define CBOR_EXPORT __declspec(dllimport)
#endif  // defined(CBOR_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(CBOR_IMPLEMENTATION)
#define CBOR_EXPORT __attribute__((visibility("default")))
#else
#define CBOR_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define CBOR_EXPORT
#endif

#endif  // COMPONENTS_CBOR_CBOR_EXPORT_H_
