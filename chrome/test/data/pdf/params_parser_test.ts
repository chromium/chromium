// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FittingType, OpenPdfParamsParser, PluginController, ViewMode} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

import {createMockPdfPluginForTest} from './test_util.js';

const URL = 'http://xyz.pdf';

function getParamsParser(): OpenPdfParamsParser {
  const getPageBoundingBoxCallback = function(pageIndex: number) {
    return Promise.resolve(
        {x: 10 + pageIndex, y: 15 + pageIndex, width: 200, height: 300});
  };
  const paramsParser = new OpenPdfParamsParser(function(destination: string) {
    // Set the dummy viewport dimensions for calculating the zoom level for
    // view destination with 'FitR' type.
    paramsParser.setViewportDimensions({width: 300, height: 500});

    if (destination === 'RU') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_1', pageIndex: 26});
    }
    if (destination === 'US') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_2', pageIndex: 0});
    }
    if (destination === 'UY') {
      return Promise.resolve(
          {messageId: 'getNamedDestination_3', pageIndex: 22});
    }
    if (destination === 'DestWithXYZ') {
      return Promise.resolve({
        messageId: 'getNamedDestination_4',
        namedDestinationView: `${ViewMode.XYZ},111,222,1.7`,
        pageIndex: 10,
      });
    }
    if (destination === 'DestWithXYZAtZoomNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_5',
        namedDestinationView: `${ViewMode.XYZ},111,222,null`,
        pageIndex: 10,
      });
    }
    if (destination === 'DestWithXYZWithX0') {
      return Promise.resolve({
        messageId: 'getNamedDestination_6',
        namedDestinationView: `${ViewMode.XYZ},0,200,1.7`,
        pageIndex: 11,
      });
    }
    if (destination === 'DestWithXYZWithXNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_7',
        namedDestinationView: `${ViewMode.XYZ},null,200,1.7`,
        pageIndex: 11,
      });
    }
    if (destination === 'DestWithXYZWithY0') {
      return Promise.resolve({
        messageId: 'getNamedDestination_8',
        namedDestinationView: `${ViewMode.XYZ},100,0,1.7`,
        pageIndex: 11,
      });
    }
    if (destination === 'DestWithXYZWithYNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_9',
        namedDestinationView: `${ViewMode.XYZ},100,null,1.7`,
        pageIndex: 11,
      });
    }
    if (destination === 'DestWithFitR') {
      return Promise.resolve({
        messageId: 'getNamedDestination_10',
        namedDestinationView: `${ViewMode.FIT_R},20,100,120,300`,
        pageIndex: 0,
      });
    }
    if (destination === 'DestWithFitRReversedCoordinates') {
      return Promise.resolve({
        messageId: 'getNamedDestination_11',
        namedDestinationView: `${ViewMode.FIT_R},120,300,20,100`,
        pageIndex: 0,
      });
    }
    if (destination === 'DestWithFitRWithNull') {
      return Promise.resolve({
        messageId: 'getNamedDestination_12',
        namedDestinationView: `${ViewMode.FIT_R},null,100,100,300`,
        pageIndex: 0,
      });
    }
    if (destination === 'DestWithFitB') {
      return Promise.resolve({
        messageId: 'getNamedDestination_13',
        namedDestinationView: `${ViewMode.FIT_B}`,
        pageIndex: 2,
      });
    }
    return Promise.resolve(
        {messageId: 'getNamedDestination_14', pageIndex: -1});
  }, getPageBoundingBoxCallback);
  return paramsParser;
}

chrome.test.runTests([
  /**
   * Test various open parameters.
   */
  async function testParamsParser() {
    const paramsParser = getParamsParser();

    // Checking #nameddest.
    let params = await paramsParser.getViewportFromUrlParams(`${URL}#RU`);
    chrome.test.assertEq(26, params.pageIndex);

    // Checking #nameddest=name.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#nameddest=US`);
    chrome.test.assertEq(0, params.pageIndex);

    // Checking #page=pagenum without setting the page count should not have a
    // page value.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=6`);
    chrome.test.assertEq(null, params.pageIndex);

    // Checking #page=pagenum nameddest. The document first page has a pagenum
    // value of 1.
    paramsParser.setPageCount(100);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=6`);
    chrome.test.assertEq(5, params.pageIndex);

    // Checking #zoom=scale.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#zoom=200`);
    chrome.test.assertEq(2, params.zoom);

    // Checking #zoom=scale,left,top.
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#zoom=200,100,200`);
    chrome.test.assertEq(2, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    // Checking #nameddest=name and zoom=scale.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=UY&zoom=150`);
    chrome.test.assertEq(22, params.pageIndex);
    chrome.test.assertEq(1.5, params.zoom);

    // Checking #page=pagenum and zoom=scale.
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#page=2&zoom=250`);
    chrome.test.assertEq(1, params.pageIndex);
    chrome.test.assertEq(2.5, params.zoom);

    // Checking #nameddest=name and zoom=scale,left,top.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=UY&zoom=150,100,200`);
    chrome.test.assertEq(22, params.pageIndex);
    chrome.test.assertEq(1.5, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    // Checking #page=pagenum and zoom=scale,left,top.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#page=2&zoom=250,100,200`);
    chrome.test.assertEq(1, params.pageIndex);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);

    // Checking #page=pagenum with value out of upper bounds sets the value to
    // the upper bound.
    paramsParser.setPageCount(5);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=6`);
    chrome.test.assertEq(4, params.pageIndex);

    // Checking #page=pagenum with value out of lower bounds sets the value to
    // the lower bound.
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=0`);
    chrome.test.assertEq(0, params.pageIndex);

    // Checking #page=pagenum with a page count set to 0 should not have a page
    // value.
    paramsParser.setPageCount(0);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#page=1`);
    chrome.test.assertEq(null, params.pageIndex);

    chrome.test.succeed();
  },
  /**
   * Test named destinations that specify view fit types.
   */
  async function testParamsNamedDestWithViewFit() {
    const paramsParser = getParamsParser();

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with multiple valid parameters.
    let params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZ`);
    chrome.test.assertEq(10, params.pageIndex);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(111, params.position!.x);
    chrome.test.assertEq(222, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" with a zoom parameter of null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZAtZoomNull`);
    chrome.test.assertEq(10, params.pageIndex);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(111, params.position!.x);
    chrome.test.assertEq(222, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its X parameter is 0.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithX0`);
    chrome.test.assertEq(11, params.pageIndex);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(0, params.position!.x);
    chrome.test.assertEq(200, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its X parameter is null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithXNull`);
    chrome.test.assertEq(11, params.pageIndex);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertTrue(Number.isNaN(params.position!.x));
    chrome.test.assertEq(200, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its Y parameter is 0.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithY0`);
    chrome.test.assertEq(11, params.pageIndex);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertEq(0, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "XYZ" and its Y parameter is null.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithXYZWithYNull`);
    chrome.test.assertEq(11, params.pageIndex);
    chrome.test.assertEq(1.7, params.zoom);
    chrome.test.assertEq(100, params.position!.x);
    chrome.test.assertTrue(Number.isNaN(params.position!.y));
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitR`);
    chrome.test.assertEq(0, params.pageIndex);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(20, params.position!.x);
    chrome.test.assertEq(100, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with multiple valid parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitRReversedCoordinates`);
    chrome.test.assertEq(0, params.pageIndex);
    chrome.test.assertEq(2.5, params.zoom);
    chrome.test.assertEq(20, params.position!.x);
    chrome.test.assertEq(100, params.position!.y);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitR" with one NULL parameters.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitRWithNull`);
    chrome.test.assertEq(0, params.pageIndex);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #nameddest=name with a nameddest that specifies the view fit
    // type is "FitB".
    paramsParser.setPageCount(3);
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#nameddest=DestWithFitB`);
    chrome.test.assertEq(2, params.pageIndex);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(12, params.boundingBox.x);
    chrome.test.assertEq(17, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);

    chrome.test.succeed();
  },
  /**
   * Test view params.
   */
  async function testParamsView() {
    const paramsParser = getParamsParser();

    // Checking #view=Fit.
    let params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT}`);
    chrome.test.assertEq(FittingType.FIT_TO_PAGE, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitH.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_H}`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitH,[int position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_H},789`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(789, params.viewPosition);

    // Checking #view=FitH,[float position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_H},7.89`);
    chrome.test.assertEq(FittingType.FIT_TO_WIDTH, params.view);
    chrome.test.assertEq(7.89, params.viewPosition);

    // Checking #view=FitV.
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_V}`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=FitV,[int position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_V},123`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(123, params.viewPosition);

    // Checking #view=FitV,[float position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_V},1.23`);
    chrome.test.assertEq(FittingType.FIT_TO_HEIGHT, params.view);
    chrome.test.assertEq(1.23, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitW`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitW,555`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.XYZ}`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.XYZ},111,222,1.7`);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_R}`);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    // Checking #view=[wrong parameter],[position].
    params = await paramsParser.getViewportFromUrlParams(
        `${URL}#view=${ViewMode.FIT_R},20,100,120,300`);
    chrome.test.assertEq(undefined, params.zoom);
    chrome.test.assertEq(undefined, params.position);
    chrome.test.assertEq(undefined, params.view);
    chrome.test.assertEq(undefined, params.viewPosition);

    chrome.test.succeed();
  },
  /**
   * Test toolbar and navpane params.
   */
  function testParamsToolbarAndNavpane() {
    const paramsParser = getParamsParser();

    // Checking #toolbar=0 to disable the toolbar.
    chrome.test.assertFalse(paramsParser.shouldShowToolbar(`${URL}#toolbar=0`));
    chrome.test.assertTrue(paramsParser.shouldShowToolbar(`${URL}#toolbar=1`));

    // Checking #navpanes=0 to collapse the sidenav.
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0`, false));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0`, true));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1`, false));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1`, true));

    // Checking #navpanes=0&toolbars=1 shows the toolbar and a collapsed
    // sidenav.
    chrome.test.assertTrue(
        paramsParser.shouldShowToolbar(`${URL}#navpanes=0&toolbar=1`));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0&toolbar=1`, false));
    chrome.test.assertFalse(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=0&toolbar=1`, true));

    // Checking #navpanes=1&toolbars=0 shows the toolbar and the sidenav.
    chrome.test.assertTrue(
        paramsParser.shouldShowToolbar(`${URL}#navpanes=1&toolbar=0`));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1&toolbar=0`, false));
    chrome.test.assertTrue(
        paramsParser.shouldShowSidenav(`${URL}#navpanes=1&toolbar=0`, true));

    // Checking no relevant parameters defaults to !sidenavCollapsed.
    chrome.test.assertFalse(paramsParser.shouldShowSidenav(`${URL}`, true));
    chrome.test.assertTrue(paramsParser.shouldShowSidenav(`${URL}`, false));

    chrome.test.succeed();
  },
  async function testParamsViewFitB() {
    const paramsParser = getParamsParser();

    // Checking #view=FitB.
    let params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitB`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(0);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitB`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(1);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitB`);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(10, params.boundingBox.x);
    chrome.test.assertEq(15, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);

    chrome.test.succeed();
  },
  async function testParamsViewFitBH() {
    const paramsParser = getParamsParser();

    // Checking #view=FitBH.
    let params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBH`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(0);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBH`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(1);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBH`);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX_WIDTH, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(10, params.boundingBox.x);
    chrome.test.assertEq(15, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);

    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBH,100`);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX_WIDTH, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(10, params.boundingBox.x);
    chrome.test.assertEq(15, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);
    chrome.test.assertEq(100, params.viewPosition);

    chrome.test.succeed();
  },
  async function testParamsViewFitBV() {
    const paramsParser = getParamsParser();

    // Checking #view=FitBV.
    let params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBV`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(0);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBV`);
    chrome.test.assertEq(null, params.view);
    chrome.test.assertEq(null, params.boundingBox);

    paramsParser.setPageCount(1);
    params = await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBV`);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX_HEIGHT, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(10, params.boundingBox.x);
    chrome.test.assertEq(15, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);

    params =
        await paramsParser.getViewportFromUrlParams(`${URL}#view=FitBV,100`);
    chrome.test.assertEq(FittingType.FIT_TO_BOUNDING_BOX_HEIGHT, params.view);
    chrome.test.assertTrue(params.boundingBox !== undefined);
    chrome.test.assertEq(10, params.boundingBox.x);
    chrome.test.assertEq(15, params.boundingBox.y);
    chrome.test.assertEq(200, params.boundingBox.width);
    chrome.test.assertEq(300, params.boundingBox.height);
    chrome.test.assertEq(100, params.viewPosition);

    chrome.test.succeed();
  },
  async function testParamsTextFragments() {
    const paramsParser = getParamsParser();

    async function assertTextFragments(fragment: string, expected?: string[]) {
      const params =
          await paramsParser.getViewportFromUrlParams(`${URL}${fragment}`);
      chrome.test.assertEq(expected, params.textFragments);
    }

    // Checking single text fragment.
    await assertTextFragments('#:~:text=apples', ['apples']);

    // Checking multiple text fragments.
    await assertTextFragments(
        '#:~:text=apples&text=oranges&text=hello-,world,there,-world',
        ['apples', 'oranges', 'hello-,world,there,-world']);

    // Checking case where no text fragments are present.
    await assertTextFragments('#page=3', undefined);

    // Check case where only delimiter is present.
    await assertTextFragments('#:~:', undefined);

    // Check case where there are other viewport parameters before the text
    // fragments.
    await assertTextFragments(
        '#page=3&view=FitBV:~:text=foo&text=bar', ['foo', 'bar']);

    // Check case where text fragment could have percent-encoded values that
    // are used as part of the prefix / suffix like `,` and `-`.
    await assertTextFragments(
        '#page=3&view=FitBV:~:text=foo%2C-,apples,-bar%2D&text=bar%2D',
        ['foo%2C-,apples,-bar%2D', 'bar%2D']);

    // Check case where text fragments are empty.
    await assertTextFragments(
        '#page=3&view=FitBV:~:text=&text=&text=', undefined);

    // Check case where some text fragments are empty.
    await assertTextFragments(
        '#page=3&view=FitBV:~:text=&text=foo%2C-,apples,-bar%2D&text=',
        ['foo%2C-,apples,-bar%2D']);

    // Check case where URL fragment is invalid and contains multiple
    // octothorpes.
    await assertTextFragments(
        '#page=3&view=FitBV#:~:text=&text=foo%2C-,apples,-bar%2D&text=',
        undefined);

    chrome.test.succeed();
  },
  async function testParamsSearch() {
    const paramsParser = getParamsParser();
    paramsParser.setPageCount(100);

    async function assertSearch(fragment: string, expected?: string[]) {
      const params =
          await paramsParser.getViewportFromUrlParams(`${URL}#${fragment}`);
      chrome.test.assertEq(expected, params.searchTerms);
      return params;
    }

    await assertSearch('search="apple"', ['apple']);
    await assertSearch(
        'search="apple banana cherry"', ['apple', 'banana', 'cherry']);
    await assertSearch('search=apple+banana', ['apple', 'banana']);
    await assertSearch('search="apple   banana"&phrase=TRUE', ['apple banana']);
    await assertSearch(
        'search="apple banana"&phrase=false', ['apple', 'banana']);
    await assertSearch(
        'search="apple banana"&phrase=invalid', ['apple', 'banana']);
    await assertSearch('search=', undefined);
    await assertSearch('search="   "', undefined);
    const emptyNamedDest = await assertSearch('RU=', undefined);
    chrome.test.assertEq(undefined, emptyNamedDest.pageIndex);
    const withDirective =
        await assertSearch('search="apple":~:text=oranges', undefined);
    chrome.test.assertEq(['oranges'], withDirective.textFragments);
    await assertSearch('search="apple":~:text=', ['apple']);

    const combined =
        await assertSearch('page=3&zoom=200&search="apple"', ['apple']);
    chrome.test.assertEq(2, combined.pageIndex);
    chrome.test.assertEq(2, combined.zoom);

    chrome.test.succeed();
  },
  async function testHandleUrlParams() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const mockPlugin = createMockPdfPluginForTest();
    PluginController.getInstance().setPluginForTesting(mockPlugin);

    const paramsParser = getParamsParser();
    paramsParser.setPageCount(10);

    // #search with phrase=true highlights exact phrase from page 0.
    viewer.handleUrlParams(await paramsParser.getViewportFromUrlParams(
        `${URL}#search="Second%20Section"&phrase=true`));
    chrome.test.assertEq(
        {
          type: 'highlightSearchTerms',
          searchTerms: ['Second Section'],
          startPageIndex: 0,
        },
        mockPlugin.findMessage('highlightSearchTerms'));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('highlightTextFragments'));

    // #page=3 sets the search start page index to 2.
    mockPlugin.clearMessages();
    viewer.handleUrlParams(await paramsParser.getViewportFromUrlParams(
        `${URL}#page=3&search="Subsection"`));
    chrome.test.assertEq(
        {
          type: 'highlightSearchTerms',
          searchTerms: ['Subsection'],
          startPageIndex: 2,
        },
        mockPlugin.findMessage('highlightSearchTerms'));

    // #:~:text= takes priority over #search=.
    mockPlugin.clearMessages();
    viewer.handleUrlParams(await paramsParser.getViewportFromUrlParams(
        `${URL}#search="Subsection":~:text=Second%20Section`));
    chrome.test.assertEq(
        {type: 'highlightTextFragments', textFragments: ['Second%20Section']},
        mockPlugin.findMessage('highlightTextFragments'));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('highlightSearchTerms'));

    // Navigating without #search= or #:~:text= clears prior highlights once.
    mockPlugin.clearMessages();
    viewer.handleUrlParams(
        await paramsParser.getViewportFromUrlParams(`${URL}#page=1`));
    chrome.test.assertEq(
        {type: 'highlightTextFragments', textFragments: []},
        mockPlugin.findMessage('highlightTextFragments'));

    mockPlugin.clearMessages();
    viewer.handleUrlParams(
        await paramsParser.getViewportFromUrlParams(`${URL}#page=1`));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('highlightTextFragments'));
    chrome.test.assertEq(
        undefined, mockPlugin.findMessage('highlightSearchTerms'));

    chrome.test.succeed();
  },
]);
