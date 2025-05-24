// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Minimal required ticket.
const minimal_ticket = {
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

// Ticket with margins and scale.
const ticket_with_margins_and_scale = {
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
    collate: {collate: false},
    fit_to_page: {type: 'FIT'},
    margins: {
      top_microns: 1003,
      right_microns: 3002,
      bottom_microns: 5008,
      left_microns: 3050
    }
  }
};

function formatPrintJobRequest(printerId, title, arrayBuffer, ticket) {
  return {
    job: {
      printerId: printerId,
      title: title,
      ticket: ticket,
      contentType: 'application/pdf',
      document:
          new Blob([new Uint8Array(arrayBuffer)], {type: 'application/pdf'})
    }
  };
}

function submitJob(printerId, title, url, ticket, callback) {
  fetch(url).then(response => response.arrayBuffer()).then(arrayBuffer => {
    const submitJobRequest =
        formatPrintJobRequest(printerId, title, arrayBuffer, ticket);
    chrome.printing.submitJob(submitJobRequest, callback);
  });
}

async function submitJobPromise(printerId, title, url, ticket) {
  let response = await fetch(url);
  let arrayBuffer = await response.arrayBuffer();
  const submitJobRequest =
      formatPrintJobRequest(printerId, title, arrayBuffer, ticket);
  return chrome.printing.submitJob(submitJobRequest);
}
