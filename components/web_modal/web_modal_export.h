// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_MODAL_WEB_MODAL_EXPORT_H_
#define COMPONENTS_WEB_MODAL_WEB_MODAL_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(WEB_MODAL_IMPLEMENTATION)
#define WEB_MODAL_EXPORT __declspec(dllexport)
#else
#define WEB_MODAL_EXPORT __declspec(dllimport)
#endif  // defined(WEB_MODAL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(WEB_MODAL_IMPLEMENTATION)
#define WEB_MODAL_EXPORT __attribute__((visibility("default")))
#else
#define WEB_MODAL_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define WEB_MODAL_EXPORT
#endif

#endif  // COMPONENTS_WEB_MODAL_WEB_MODAL_EXPORT_H_
