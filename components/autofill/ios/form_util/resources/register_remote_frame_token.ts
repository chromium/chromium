// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers a remote frame token associated to the frame ID. This
 * allows us to map page content world frames to isolated content world ones in
 * the browser layer.
 */

// Requires functions from child_frame_registration_lib.ts.

import {setRemoteFrameToken} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {generateRandomId} from '//ios/web/public/js_messaging/resources/frame_id.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

function registerRemoteToken(): void {
  const remoteFrameToken = generateRandomId();
  // Store the remote token in the DOM. Page content world scripts will be able
  // to read it and send it in the payload of their messages to the browser. The
  // browser layer uses remote tokens to map page content world frames to their
  // isolated world counter parts, which is where the rest of Autofill lives.
  setRemoteFrameToken(remoteFrameToken);
  gCrWeb.remoteFrameRegistration.registerSelfWithRemoteToken(remoteFrameToken);
}


// Register a new token after the page is displayed due to navigation. The
// browser layer destroys existing WebFrame's on navigation which removes any
// registered frame tokens. That is why the frame must be registered again if
// the user navigates back to it.
addEventListener('pageshow', () => {
  registerRemoteToken();
});
