// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OpenPdfParamsParser} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/open_pdf_params_parser.js';
import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_fitting_type.js';
import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';


const tests = [
  /**
   * Test named destinations.
   */
  function testParamsParser() {
    const paramsParser = new OpenPdfParamsParser(function(destination) {
      if (destination == 'RU') {
        paramsParser.onNamedDestinationReceived(26);
      } else if (destination == 'US') {
        paramsParser.onNamedDestinationReceived(0);
      } else if (destination == 'UY') {
        paramsParser.onNamedDestinationReceived(22);
      } else {
        paramsParser.onNamedDestinationReceived(-1);
      }
    });

    const url = 'http://xyz.pdf';

    // Checking #nameddest.
    paramsParser.getViewportFromUrlParams(`${url}#RU`, function(params) {
      chrome.test.assertEq(26, params.page);
    });

    // Checking #nameddest=name.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=US`, function(params) {
          chrome.test.assertEq(0, params.page);
        });

    // Checking #page=pagenum nameddest.The document first page has a pagenum
    // value of 1.
    paramsParser.getViewportFromUrlParams(`${url}#page=6`, function(params) {
      chrome.test.assertEq(5, params.page);
    });

    // Checking #zoom=scale.
    paramsParser.getViewportFromUrlParams(`${url}#zoom=200`, function(params) {
      chrome.test.assertEq(2, params.zoom);
    });

    // Checking #zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        `${url}#zoom=200,100,200`, function(params) {
          chrome.test.assertEq(2, params.zoom);
          chrome.test.assertEq(100, params.position.x);
          chrome.test.assertEq(200, params.position.y);
        });

    // Checking #nameddest=name and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=UY&zoom=150`, function(params) {
          chrome.test.assertEq(22, params.page);
          chrome.test.assertEq(1.5, params.zoom);
        });

    // Checking #page=pagenum and zoom=scale.
    paramsParser.getViewportFromUrlParams(
        `${url}#page=2&zoom=250`, function(params) {
          chrome.test.assertEq(1, params.page);
          chrome.test.assertEq(2.5, params.zoom);
        });

    // Checking #nameddest=name and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=UY&zoom=150,100,200`, function(params) {
          chrome.test.assertEq(22, params.page);
          chrome.test.assertEq(1.5, params.zoom);
          chrome.test.assertEq(100, params.position.x);
          chrome.test.assertEq(200, params.position.y);
        });

    // Checking #page=pagenum and zoom=scale,left,top.
    paramsParser.getViewportFromUrlParams(
        `${url}#page=2&zoom=250,100,200`, function(params) {
          chrome.test.assertEq(1, params.page);
          chrome.test.assertEq(2.5, params.zoom);
          chrome.test.assertEq(100, params.position.x);
          chrome.test.assertEq(200, params.position.y);
        });

    // Checking #view=Fit.
    paramsParser.getViewportFromUrlParams(`${url}#view=Fit`, function(params) {
      chrome.test.assertEq(FittingType.FIT_TO_PAGE, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=FitH.
    paramsParser.getViewportFromUrlParams(`${url}#view=FitH`, function(params) {
      chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=FitH,[int position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitH,789`, function(params) {
          chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
          chrome.test.assertEq(789, params.viewPosition);
        });
    // Checking #view=FitH,[float position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitH,7.89`, function(params) {
          chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
          chrome.test.assertEq(7.89, params.viewPosition);
        });
    // Checking #view=FitV.
    paramsParser.getViewportFromUrlParams(`${url}#view=FitV`, function(params) {
      chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=FitV,[int position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitV,123`, function(params) {
          chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
          chrome.test.assertEq(123, params.viewPosition);
        });
    // Checking #view=FitV,[float position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitV,1.23`, function(params) {
          chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
          chrome.test.assertEq(1.23, params.viewPosition);
        });
    // Checking #view=[wrong parameter].
    paramsParser.getViewportFromUrlParams(`${url}#view=FitW`, function(params) {
      chrome.test.assertEq(undefined, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=[wrong parameter],[position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitW,555`, function(params) {
          chrome.test.assertEq(undefined, params.view);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #toolbar=0 to disable the toolbar.
    let uiParams = paramsParser.getUiUrlParams(`${url}#toolbar=0`);
    chrome.test.assertFalse(uiParams.toolbar);
    uiParams = paramsParser.getUiUrlParams(`${url}#toolbar=1`);
    chrome.test.assertTrue(uiParams.toolbar);

    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
