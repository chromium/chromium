// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getBrowser, logMessage, readStream} from '../client.js';
import {$} from '../page_element_types.js';

$.getpagecontext.addEventListener('click', async () => {
  logMessage('Starting Get Page Context');
  const options: any = {};
  if ($.innerTextCheckbox.checked) {
    options.innerText = true;
  }
  const textLimit = Number.parseInt($.innerTextBytesLimit.value);
  if (!Number.isNaN(textLimit)) {
    options.innerTextBytesLimit = textLimit;
  }
  if ($.viewportScreenshotCheckbox.checked) {
    options.viewportScreenshot = true;
  }
  if ($.pdfDataCheckbox.checked) {
    options.pdfData = true;
  }
  if ($.annotatedPageContentCheckbox.checked) {
    options.annotatedPageContent = true;
  }
  $.faviconImg.src = '';
  $.screenshotImg.src = '';
  try {
    const pageContent = await getBrowser()!.getContextFromFocusedTab!(options);
    if (pageContent.viewportScreenshot) {
      const blob =
          new Blob([pageContent.viewportScreenshot.data], {type: 'image/jpeg'});
      $.screenshotImg.src = URL.createObjectURL(blob);
    }
    if (pageContent.tabData.favicon) {
      const favicon = await pageContent.tabData.favicon();
      if (favicon) {
        $.faviconImg.src = URL.createObjectURL(favicon);
      }
    }
    if (pageContent.pdfDocumentData) {
      const pdfOrigin = pageContent.pdfDocumentData.origin;
      const pdfSizeLimitExceeded =
          pageContent.pdfDocumentData.pdfSizeLimitExceeded;
      let pdfDataSize = 0;
      if (pageContent.pdfDocumentData.pdfData) {
        pdfDataSize =
            (await readStream(pageContent.pdfDocumentData.pdfData!)).length;
      }
      $.getPageContextResult.innerText =
          `Got ${pdfDataSize} bytes of PDF data(origin = ${
              pdfOrigin}, sizeLimitExceeded = ${pdfSizeLimitExceeded})`;
    }
    if (pageContent.annotatedPageData &&
        pageContent.annotatedPageData.annotatedPageContent) {
      const annotatedPageDataSize =
          (await readStream(pageContent.annotatedPageData.annotatedPageContent))
              .length;
      $.getPageContextResult.innerText =
          `Annotated page content data length: ${annotatedPageDataSize}`;
    }
    $.getPageContextStatus.innerText = 'Finished Get Page Context.';
    $.getPageContextResult.innerText =
        `Returned data: ${JSON.stringify(pageContent, null, 2)}`;
  } catch (error) {
    $.getPageContextStatus.innerText = `Error getting page context: ${error}`;
  }
});
