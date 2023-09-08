// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for google-photos-shared-album-dialog component.
 */

import 'chrome://personalization/strings.m.js';

import {AcceptEvent, GooglePhotosSharedAlbumDialogElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {initElement} from './personalization_app_test_utils.js';

suite('GooglePhotosSharedAlbumDialogElementTest', function() {
  let dialogElement: GooglePhotosSharedAlbumDialogElement|null;

  setup(() => {
    loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
  });

  teardown(async () => {
    if (dialogElement) {
      dialogElement.remove();
    }
    dialogElement = null;
    await flushTasks();
  });

  test('sends accept event when accept is clicked', async () => {
    dialogElement = initElement(GooglePhotosSharedAlbumDialogElement);
    await waitAfterNextRender(dialogElement);

    const acceptEvent = eventToPromise(AcceptEvent.EVENT_NAME, dialogElement);
    dialogElement.shadowRoot!.getElementById('accept')!.click();
    await acceptEvent;
  });

  test('sends close and cancel event when cancel is clicked', async () => {
    dialogElement = initElement(GooglePhotosSharedAlbumDialogElement);
    await waitAfterNextRender(dialogElement);

    const closeEvent = eventToPromise('close', dialogElement);
    const cancelEvent = eventToPromise('cancel', dialogElement);

    dialogElement.shadowRoot!.getElementById('close')!.click();

    await closeEvent;
    await cancelEvent;
  });
});
