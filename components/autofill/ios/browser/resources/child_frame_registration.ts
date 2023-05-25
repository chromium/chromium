// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Identifies relationships between parent and child frames
 * by generating a unique ID and sending it to the browser from each frame.
 */

import {generateRandomId, getFrameId} from '//ios/web/public/js_messaging/resources/frame_id.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// TODO(crbug.com/1440471): This just posts a message with the frame ID and a
// mock value for the remote token. This needs to message the child frames.

sendWebKitMessage('RegisterChildFrame', {
  'local_frame_id': getFrameId(),
  'remote_frame_id': generateRandomId(),
});
