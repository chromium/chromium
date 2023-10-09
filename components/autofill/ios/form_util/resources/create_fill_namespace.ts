// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Namespace for this file. It depends on |gCrWeb| having already been
 * injected. String 'fill' is used in |gCrWeb['fill']| as it needs to be
 * accessed in Objective-C code.
 */
gCrWeb.fill = {};

// Store fill namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
gCrWeb['fill'] = gCrWeb.fill;
