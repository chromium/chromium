// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, logMessage, readStream} from '../client.js';
import {$} from '../page_element_types.js';

$.executeAction.addEventListener('click', async () => {
  logMessage('Starting Execute Action');

  // The action proto is expected to be a BrowserAction proto, which is binary
  // serialized and then base64 encoded.
  const protoByteString = atob($.actionProtoEncodedText.value);
  const protoBytes = new Uint8Array(protoByteString.length);
  for (let i = 0; i < protoByteString.length; i++) {
    protoBytes[i] = protoByteString.charCodeAt(i);
  }

  const params: any = {
    actionProto: protoBytes.buffer,
    tabContextOptions: {annotatedPageContent: true, viewportScreenshot: true},
  };

  $.actionUpdatedContextResult.innerText = '';
  $.actionUpdatedScreenshotImg.src = '';
  try {
    const actionResult = await client!.browser!.actInFocusedTab!(params);
    const pageContent = actionResult.tabContextResult;
    if (pageContent) {
      if (pageContent.viewportScreenshot) {
        const blob = new Blob(
            [pageContent.viewportScreenshot.data], {type: 'image/jpeg'});
        $.actionUpdatedScreenshotImg.src = URL.createObjectURL(blob);
      }
      if (pageContent.annotatedPageData &&
          pageContent.annotatedPageData.annotatedPageContent) {
        const annotatedPageDataSize =
            (await readStream(
                 pageContent.annotatedPageData.annotatedPageContent))
                .length;
        $.actionUpdatedContextResult.innerText =
            `Annotated page content data length: ${annotatedPageDataSize}`;
      }
    }
    $.actionStatus.innerText = 'Finished Execute Action.';
    $.actionUpdatedContextResult.innerText +=
        `Returned data: ${JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Execute Action: ${error}`;
  }
});
