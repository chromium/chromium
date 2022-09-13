// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LOGIN_LOGIN_EXPORT_H_
#define COMPONENTS_LOGIN_LOGIN_EXPORT_H_

#if defined(COMPONENT_BUILD)

#if defined(WIN32)

#if defined(LOGIN_IMPLEMENTATION)
#define LOGIN_EXPORT __declspec(dllexport)
#else
#define LOGIN_EXPORT __declspec(dllimport)
#endif  // defined(LOGIN_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(LOGIN_IMPLEMENTATION)
#define LOGIN_EXPORT __attribute__((visibility("default")))
#else
#define LOGIN_EXPORT
#endif  // defined(LOGIN_IMPLEMENTATION)

#endif  // defined(WIN32)

#else  // defined(COMPONENT_BUILD)

#define LOGIN_EXPORT

#endif

#endif  // COMPONENTS_LOGIN_LOGIN_EXPORT_H_
