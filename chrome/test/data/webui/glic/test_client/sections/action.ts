// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {client, logMessage, readStream} from '../client.js';
import {$} from '../page_element_types.js';


$.executeAction.addEventListener('click', async () => {
  logMessage('Starting Execute Action');

  // The action proto is expected to be a BrowserAction proto, which is binary
  // serialized and then base64 encoded.
  const protoBytes = Uint8Array.fromBase64($.actionProtoEncodedText.value);

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
    $.actionStatus.innerText =
        `Finished Execute Action. Result code ${actionResult.actionResult}.`;
    $.actionUpdatedContextResult.innerText +=
        `Returned data: ${JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Execute Action: ${error}`;
  }
});

$.createActorTask.addEventListener('click', async () => {
  logMessage('Starting Create Actor Task');
  try {
    const taskId = await client!.browser!.createTask!();
    $.actorTaskId.value = taskId.toString();
    $.actionStatus.innerText = `Created task with ID: ${taskId}`;
  } catch (error) {
    $.actionStatus.innerText = `Error in Create Actor Task: ${error}`;
  }
});

$.stopActorTask.addEventListener('click', () => {
  logMessage('Starting Stop Actor Task');
  const taskIdStr = $.actorTaskId.value;
  if (taskIdStr) {
    const taskId = parseInt(taskIdStr, 10);
    if (isNaN(taskId)) {
      $.actionStatus.innerText = `Invalid task ID: ${taskIdStr}`;
      return;
    }
    client!.browser!.stopActorTask!(taskId);
    $.actionStatus.innerText = `Stopped task with ID: ${taskId}`;
  } else {
    client!.browser!.stopActorTask!();
    $.actionStatus.innerText = 'Stopped most recent task.';
  }
});
