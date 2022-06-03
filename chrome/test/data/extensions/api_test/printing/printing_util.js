// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function submitJob(printerId, title, url, callback) {
  // Minimal required ticket.
  const ticket = {
    version: '1.0',
    print: {
      color: {type: 'STANDARD_COLOR'},
      duplex: {type: 'NO_DUPLEX'},
      page_orientation: {type: 'LANDSCAPE'},
      copies: {copies: 1},
      dpi: {horizontal_dpi: 300, vertical_dpi: 400},
      media_size: {
        width_microns: 210000,
        height_microns: 297000,
        vendor_id: 'iso_a4_210x297mm'
      },
      collate: {collate: false}
    }
  };

  fetch(url).then(response => response.arrayBuffer()).then(arrayBuffer => {
    const submitJobRequest = {
      job: {
        printerId: printerId,
        title: title,
        ticket: ticket,
        contentType: 'application/pdf',
        document:
            new Blob([new Uint8Array(arrayBuffer)], {type: 'application/pdf'})
      }
    };
    chrome.printing.submitJob(submitJobRequest, callback);
  });
}
