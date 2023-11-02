// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_EXPORT_H_
#define COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_EXPORT_H_

#if defined(COMPONENT_BUILD) && defined(ON_LOAD_SCRIPT_INJECTOR_IMPLEMENTATION)
#define ON_LOAD_SCRIPT_INJECTOR_EXPORT __attribute__((visibility("default")))
#else  // !defined(COMPONENT_BUILD) ||
       // !defined(ON_LOAD_SCRIPT_INJECTOR_IMPLEMENTATION)
#define ON_LOAD_SCRIPT_INJECTOR_EXPORT
#endif

#endif  // COMPONENTS_ON_LOAD_SCRIPT_INJECTOR_EXPORT_H_
