// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_INCLUDE_CRONET_EXPORT_H_
#define COMPONENTS_CRONET_NATIVE_INCLUDE_CRONET_EXPORT_H_

#if defined(WIN32)
#define CRONET_EXPORT __declspec(dllexport)
#else
#define CRONET_EXPORT __attribute__((visibility("default")))
#endif

#endif  // COMPONENTS_CRONET_NATIVE_INCLUDE_CRONET_EXPORT_H_
