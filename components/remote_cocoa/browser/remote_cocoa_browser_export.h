// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_BROWSER_REMOTE_COCOA_BROWSER_EXPORT_H_
#define COMPONENTS_REMOTE_COCOA_BROWSER_REMOTE_COCOA_BROWSER_EXPORT_H_

// Defines REMOTE_COCOA_BROWSER_EXPORT so that functionality implemented by the
// RemoteCocoa browser  module can be exported to consumers.

#if defined(COMPONENT_BUILD)
#if defined(REMOTE_COCOA_BROWSER_IMPLEMENTATION)
#define REMOTE_COCOA_BROWSER_EXPORT __attribute__((visibility("default")))
#else
#define REMOTE_COCOA_BROWSER_EXPORT
#endif

#else  // defined(COMPONENT_BUILD)
#define REMOTE_COCOA_BROWSER_EXPORT
#endif

#endif  // COMPONENTS_REMOTE_COCOA_BROWSER_REMOTE_COCOA_BROWSER_EXPORT_H_
