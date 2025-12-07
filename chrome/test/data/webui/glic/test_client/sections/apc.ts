// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser, logMessage, readStream} from '../client.js';
import {$} from '../page_element_types.js';

$.copyAPCToClipboardBtn.addEventListener('click', async () => {
  let annotatedPageContent: Uint8Array<ArrayBuffer>|undefined;
  try {
    const pageContent = await getBrowser()!.getContextFromFocusedTab!
                        ({annotatedPageContent: true});
    if (pageContent.annotatedPageData?.annotatedPageContent) {
      annotatedPageContent =
          await readStream(pageContent.annotatedPageData.annotatedPageContent);
    }
  } catch (error) {
    $.APCResult.innerText = `Error getting page context: ${error}`;
    return;
  }

  if (!annotatedPageContent) {
    logMessage('fetching APC failed');
    return;
  }

  logMessage(`APC binary size in bytes: ${annotatedPageContent.length}`);

  const postResponse = await fetch(
      '/parse-apc-text', {method: 'POST', body: annotatedPageContent});
  const textproto = await postResponse.text();
  navigator.clipboard.writeText(textproto);
  $.APCResult.innerText = `APC TEXTPROTO copied to clipboard`
      + `\nFully Qualified Message Name: chrome_intelligence_proto_features.AnnotatedPageContent`;
});
