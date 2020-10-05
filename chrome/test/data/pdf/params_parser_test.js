// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/constants.js';
import {OpenPdfParamsParser} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/open_pdf_params_parser.js';
import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';


const tests = [
  /**
   * Test named destinations.
   */
  function testParamsParser() {
    const paramsParser = new OpenPdfParamsParser(function(destination) {
      // Set the dummy viewport dimensions for calculating the zoom level for
      // view destination with 'FitR' type.
      paramsParser.setViewportDimensions({width: 300, height: 500});

      if (destination === 'RU') {
        return Promise.resolve(
            {messageId: 'getNamedDestination_1', pageNumber: 26});
      } else if (destination === 'US') {
        return Promise.resolve(
            {messageId: 'getNamedDestination_2', pageNumber: 0});
      } else if (destination === 'UY') {
        return Promise.resolve(
            {messageId: 'getNamedDestination_3', pageNumber: 22});
      } else if (destination === 'DestWithXYZ') {
        return Promise.resolve({
          messageId: 'getNamedDestination_4',
          namedDestinationView: 'XYZ,111,222,1.7',
          pageNumber: 10
        });
      } else if (destination === 'DestWithXYZAtZoom0') {
        return Promise.resolve({
          messageId: 'getNamedDestination_5',
          namedDestinationView: 'XYZ,111,222,0',
          pageNumber: 10
        });
      } else if (destination === 'DestWithXYZWithNullParameter') {
        return Promise.resolve({
          messageId: 'getNamedDestination_6',
          namedDestinationView: 'XYZ,111,null,1.7',
          pageNumber: 13
        });
      } else if (destination === 'DestWithFitR') {
        return Promise.resolve({
          messageId: 'getNamedDestination_7',
          namedDestinationView: 'FitR,20,100,120,300',
          pageNumber: 0
        });
      } else if (destination === 'DestWithFitRReversedCoordinates') {
        return Promise.resolve({
          messageId: 'getNamedDestination_8',
          namedDestinationView: 'FitR,120,300,20,100',
          pageNumber: 0
        });
      } else if (destination === 'DestWithFitRWithNull') {
        return Promise.resolve({
          messageId: 'getNamedDestination_9',
          namedDestinationView: 'FitR,null,100,100,300',
          pageNumber: 0
        });
      } else {
        return Promise.resolve(
            {messageId: 'getNamedDestination_10', pageNumber: -1});
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

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with multiple valid parameters.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithXYZ`, function(params) {
          chrome.test.assertEq(10, params.page);
          chrome.test.assertEq(1.7, params.zoom);
          chrome.test.assertEq(111, params.position.x);
          chrome.test.assertEq(222, params.position.y);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with a zoom parameter of 0.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithXYZAtZoom0`, function(params) {
          chrome.test.assertEq(10, params.page);
          chrome.test.assertEq(undefined, params.zoom);
          chrome.test.assertEq(111, params.position.x);
          chrome.test.assertEq(222, params.position.y);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and one of its parameters is null.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithXYZWithNullParameter`, function(params) {
          chrome.test.assertEq(13, params.page);
          chrome.test.assertEq(undefined, params.zoom);
          chrome.test.assertEq(undefined, params.position);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithFitR`, function(params) {
          chrome.test.assertEq(0, params.page);
          chrome.test.assertEq(2.5, params.zoom);
          chrome.test.assertEq(20, params.position.x);
          chrome.test.assertEq(100, params.position.y);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithFitRReversedCoordinates`, function(params) {
          chrome.test.assertEq(0, params.page);
          chrome.test.assertEq(2.5, params.zoom);
          chrome.test.assertEq(20, params.position.x);
          chrome.test.assertEq(100, params.position.y);
          chrome.test.assertEq(undefined, params.viewPosition);
        });

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with one NULL parameters.
    paramsParser.getViewportFromUrlParams(
        `${url}#nameddest=DestWithFitRWithNull`, function(params) {
          chrome.test.assertEq(0, params.page);
          chrome.test.assertEq(undefined, params.zoom);
          chrome.test.assertEq(undefined, params.position);
          chrome.test.assertEq(undefined, params.viewPosition);
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
    // Checking #view=[wrong parameter].
    paramsParser.getViewportFromUrlParams(`${url}#view=XYZ`, function(params) {
      chrome.test.assertEq(undefined, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=[wrong parameter],[position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=XYZ,111,222,1.7`, function(params) {
          chrome.test.assertEq(undefined, params.zoom);
          chrome.test.assertEq(undefined, params.position);
          chrome.test.assertEq(undefined, params.view);
          chrome.test.assertEq(undefined, params.viewPosition);
        });
    // Checking #view=[wrong parameter].
    paramsParser.getViewportFromUrlParams(`${url}#view=FitR`, function(params) {
      chrome.test.assertEq(undefined, params.view);
      chrome.test.assertEq(undefined, params.viewPosition);
    });
    // Checking #view=[wrong parameter],[position].
    paramsParser.getViewportFromUrlParams(
        `${url}#view=FitR,20,100,120,300`, function(params) {
          chrome.test.assertEq(undefined, params.zoom);
          chrome.test.assertEq(undefined, params.position);
          chrome.test.assertEq(undefined, params.view);
          chrome.test.assertEq(undefined, params.viewPosition);
        });


    // Checking #toolbar=0 to disable the toolbar.
    chrome.test.assertFalse(paramsParser.shouldShowToolbar(`${url}#toolbar=0`));
    chrome.test.assertTrue(paramsParser.shouldShowToolbar(`${url}#toolbar=1`));

    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCompleteCallback(function() {
  chrome.test.runTests(tests);
});
