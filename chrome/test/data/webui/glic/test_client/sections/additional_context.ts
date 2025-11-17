// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AdditionalContext} from '/glic/glic_api/glic_api.js';

import {client, getBrowser} from '../client.js';
import {$} from '../page_element_types.js';

client.getInitialized().then(async () => {
  getBrowser()?.getAdditionalContext?.()?.subscribe(
      async (context: AdditionalContext) => {
        let pre = document.createElement('pre');
        $.additionalContextResult.appendChild(pre);
        if (context.name) {
          pre.innerText += `Name: ${context.name}\n`;
        }
        if (context.tabId) {
          pre.innerText += `Tab ID: ${context.tabId}\n`;
        }
        if (context.origin) {
          pre.innerText += `Origin: ${context.origin}\n`;
        }
        if (context.frameUrl) {
          pre.innerText += `URL: ${context.frameUrl}\n`;
        }
        for (const part of context.parts) {
          if (part.data) {
            pre = document.createElement('pre');
            $.additionalContextResult.appendChild(pre);
            pre.innerText += `MIME Type: ${part.data.type}\n`;
            if (part.data.type === 'image/png' ||
                part.data.type === 'image/jpeg' ||
                part.data.type === 'image/webp') {
              const i = document.createElement('img');
              i.src = URL.createObjectURL(part.data);
              $.additionalContextResult.appendChild(i);
            } else {
              pre.innerText += `Data: ${await part.data.text()}\n`;
            }
          }

          pre = document.createElement('pre');
          $.additionalContextResult.appendChild(pre);
          if (part.screenshot) {
            pre.innerText += `Screenshot: ${part.screenshot.widthPixels}x${
                part.screenshot.heightPixels} ${part.screenshot.mimeType}\n`;
          }
          if (part.webPageData) {
            pre.innerText += `Web Page Data: ${
                part.webPageData.mainDocument.innerText?.substring(
                    0, 20)}...\n`;
          }
          if (part.annotatedPageData) {
            pre.innerText += `Annotated Page Data: present\n`;
          }
          if (part.tabContext) {
            pre.innerText += 'Tab Context: present\n';
          }
          if (part.pdf) {
            pre.innerText += `PDF: present\n`;
          }
        }
      });
});
