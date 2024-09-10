// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Add type extensions needed for other scripts defining the fill namespace.
declare global {
  // Defines an additional property, `angular`, on the Window object.
  // The code below assumes that this property exists within the object.
  interface Window {
    angular: any;
  }

  // Extends the Document object to add the ability to access its
  // properties via the [] notation and defines a property that is
  // assumed to exist within the object.
  interface Document {
    [key: symbol]: number;

    __gCrWebURLNormalizer: HTMLAnchorElement;
  }
}

if (!gCrWeb.fill) {
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
}
