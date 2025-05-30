// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser, logMessage, readStream} from '../client.js';
import {$} from '../page_element_types.js';

$.copyAPCToClipboardBtn.addEventListener('click', async () => {
  let annotatedPageContent: Uint8Array|undefined = undefined;
  try {
    const pageContent = await getBrowser()!.getContextFromFocusedTab!
                        ({annotatedPageContent: true});
    if (pageContent.annotatedPageData?.annotatedPageContent) {
      annotatedPageContent =
          await readStream(pageContent.annotatedPageData.annotatedPageContent);
    }
  } catch (err) {
    logMessage(`fetching APC failed: ${err}`);
    return;
  }

  if (!annotatedPageContent) {
    logMessage('fetching APC failed');
    return;
  }

  logMessage(`APC length: ${annotatedPageContent.length}`);

  const postResponse =
      await fetch('/parse-apc', {method: 'POST', body: annotatedPageContent});
  const json = await postResponse.json();
  navigator.clipboard.writeText(JSON.stringify(json, null, 2));
  logMessage('APC copied to clipboard');
});