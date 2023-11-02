// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MACROS_H_
#define CHROME_BROWSER_VR_MACROS_H_

// This define is purely for documentation purposes. The idea is that someone
// seeing a class with functions annotated with VIRTUAL_FOR_MOCKS should think
// twice before overriding the functionality outside of tests.
#define VIRTUAL_FOR_MOCKS virtual

#endif  // CHROME_BROWSER_VR_MACROS_H_
