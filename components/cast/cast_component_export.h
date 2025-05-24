// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CAST_COMPONENT_EXPORT_H_
#define COMPONENTS_CAST_CAST_COMPONENT_EXPORT_H_

#if defined(COMPONENT_BUILD)
#define CAST_COMPONENT_EXPORT __attribute__((visibility("default")))
#else  // !defined(COMPONENT_BUILD)
#define CAST_COMPONENT_EXPORT
#endif

#endif  // COMPONENTS_CAST_CAST_COMPONENT_EXPORT_H_
