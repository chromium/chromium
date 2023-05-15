// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const CDD_EXTENSION_PRINTER = {
  'version': '1.0',
  'printer': {
    'media_size': {
      'option': [{
        'name': 'NA_LETTER',
        'width_microns': 215900,
        'height_microns': 279400,
        'is_default': true,
        'imageable_area_left_microns': 25400,
        'imageable_area_bottom_microns': 12700,
        'imageable_area_right_microns': 177800,
        'imageable_area_top_microns': 254000,
      }]
    }
  }
};

const CDD_EXTENSION_PRINTER_MISSING_PRINTABLE_AREA = {
  'version': '1.0',
  'printer': {
    'media_size': {
      'option': [{
        'name': 'ISO_A4',
        'width_microns': 210000,
        'height_microns': 297000,
        'is_default': true,
      }]
    }
  }
};

chrome.printerProvider.onGetCapabilityRequested.addListener(
    (printerId, resultCallback) => {
      let cdd = {};
      if (printerId === 'printer') {
        cdd = CDD_EXTENSION_PRINTER;
      } else if (printerId === 'printer_missing_printable_area') {
        cdd = CDD_EXTENSION_PRINTER_MISSING_PRINTABLE_AREA;
      }
      resultCallback(cdd);
    });

chrome.printerProvider.onGetPrintersRequested.addListener((resultCallback) => {
  resultCallback([
    {'id': 'printer', 'name': 'test extension printer'}, {
      'id': 'printer_missing_printable_area',
      'name': 'test extension printer missing printable area'
    }
  ]);
});
