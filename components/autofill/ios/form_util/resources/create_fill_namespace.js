// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected. String 'fill' is used in |__gCrWeb['fill']| as it needs to be
 * accessed in Objective-C code.
 */
__gCrWeb.fill = {};

// Store fill namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['fill'] = __gCrWeb.fill;
