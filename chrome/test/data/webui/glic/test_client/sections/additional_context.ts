// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AdditionalContext} from '/glic/glic_api/glic_api.js';

import {getBrowser} from '../client.js';
import {$} from '../page_element_types.js';

$.listenAdditionalContext.addEventListener('click', async () => {
  $.listenAdditionalContext.disabled = true;
  getBrowser()?.getAdditionalContext?.()?.subscribe(
      async (context: AdditionalContext) => {
        let output = '';
        if (context.name) {
          output += `Name: ${context.name}\n`;
        }
        if (context.tabId) {
          output += `Tab ID: ${context.tabId}\n`;
        }
        if (context.origin) {
          output += `Origin: ${context.origin}\n`;
        }
        if (context.frameUrl) {
          output += `URL: ${context.frameUrl}\n`;
        }
        for (const part of context.parts) {
          if (part.data) {
            output += `MIME Type: ${part.data.type}\n`;
            output += `Data: ${await part.data.text()}\n`;
          }
          if (part.screenshot) {
            output += `Screenshot: ${part.screenshot.widthPixels}x${
                part.screenshot.heightPixels} ${part.screenshot.mimeType}\n`;
          }
          if (part.webPageData) {
            output += `Web Page Data: ${
                part.webPageData.mainDocument.innerText?.substring(
                    0, 20)}...\n`;
          }
          if (part.annotatedPageData) {
            output += `Annotated Page Data: present\n`;
          }
          if (part.pdf) {
            output += `PDF: present\n`;
          }
        }
        output += '\n';
        $.additionalContextResult.value += output;
      });
});
