// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Registers a remote frame token associated to the frame ID. This
 * allows us to map page content world frames to isolated content world ones in
 * the browser layer.
 */

import {registerSelfWithRemoteToken} from '//components/autofill/ios/form_util/resources/child_frame_registration_lib.js';
import {generateRandomId} from '//ios/web/public/js_messaging/resources/frame_id.js';

/*
 Name of the html attribute used for storing the remote frame token assigned to
 the current html document.
 */
const REMOTE_FRAME_TOKEN_ATTRIBUTE = '__gChrome_remoteFrameToken';

function registerRemoteToken(): void {
  const remoteFrameToken = generateRandomId();
  // Store the remote token in the DOM. Page content world scripts will be able
  // to read it and send it in the payload of their messages to the browser. The
  // browser layer uses remote tokens to map page content world frames to their
  // isolated world counter parts, which is where the rest of Autofill lives.
  document.documentElement.setAttribute(
      REMOTE_FRAME_TOKEN_ATTRIBUTE, remoteFrameToken);
  registerSelfWithRemoteToken(remoteFrameToken);
}

// Remote token registration must be delayed until the DOM is loaded. This
// script is injected at Document End injection time.
registerRemoteToken();
