// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ORIGINAL_PATH = '/controlled_frame.html';
const TEST_PAGE_1_PATH = '/controlled_frame_api_test_page1.html';
const TEST_PAGE_2_PATH = '/controlled_frame_api_test_page2.html';

let host = '';

// Returns the full URL for the |path| using the Controlled Frame's host.
function getControlledFrameUrl(path) {
  return `http://${host}${path}`;
}

// Returns a promise that runs |callback| then resolves when |controlledFrame|'s
// loading has stopped or rejects if the loading was aborted for some reason.
function runCallbackAndWaitForLoadStop(controlledFrame, callback) {
  return new Promise(async (resolve, reject) => {
    controlledFrame.addEventListener('loadstop', () => {
      resolve('SUCCESS');
    });
    controlledFrame.addEventListener('loadabort', () => {
      reject(`ERROR: Load aborted for ${e.url} with reason ${e.reason}`);
    });
    await callback(controlledFrame);
  });
}

// Navigates |controlledFrame| to |path| and waits until loading has stopped.
async function navigateFrame(controlledFrame, path) {
  const callback = function() {
    controlledFrame.setAttribute('src', getControlledFrameUrl(path));
  };
  return await runCallbackAndWaitForLoadStop(controlledFrame, callback);
}

// Runs the test for |apiName| using |controlledFrame|.
async function testAPI(controlledFrame, apiName) {
  host = new URL(controlledFrame.src).host;
  const capitalizedApiName = apiName.charAt(0).toUpperCase() + apiName.slice(1);
  const testFunctionName = `test${capitalizedApiName}`;
  return window[testFunctionName](controlledFrame);
}

async function testBack(controlledFrame) {
  // Navigate once to be able to use the Promise version of back().
  await navigateFrame(controlledFrame, TEST_PAGE_1_PATH);
  const TEST_PAGE_1_URL = getControlledFrameUrl(TEST_PAGE_1_PATH);
  if (controlledFrame.src !== TEST_PAGE_1_URL) {
    return `ERROR: navigateFrame() failed to navigate to ${TEST_PAGE_1_URL}`;
  }

  const callPromiseBack = async function() {
    await controlledFrame.back();
  };
  await runCallbackAndWaitForLoadStop(controlledFrame, callPromiseBack);

  const ORIGINAL_URL = getControlledFrameUrl(ORIGINAL_PATH);
  if (controlledFrame.src !== ORIGINAL_URL) {
    return `ERROR: back() failed to return page to ${ORIGINAL_URL}`;
  }

  // Navigate again to test the non-promise version of the API.
  await navigateFrame(controlledFrame, TEST_PAGE_1_PATH);
  if (controlledFrame.src !== TEST_PAGE_1_URL) {
    return `ERROR: navigateFrame() failed to navigate to ${TEST_PAGE_1_URL}`;
  }

  return 'SUCCESS';
}

async function testForward(controlledFrame) {
  // Navigate once and go back to be able to use the Promise version of
  // forward().
  await navigateFrame(controlledFrame, TEST_PAGE_1_PATH);
  const TEST_PAGE_1_URL = getControlledFrameUrl(TEST_PAGE_1_PATH);
  if (controlledFrame.src !== TEST_PAGE_1_URL) {
    return `ERROR: navigateFrame() failed to navigate to ${TEST_PAGE_1_URL}`;
  }

  const callBack = async function() {
    await controlledFrame.back();
  };
  await runCallbackAndWaitForLoadStop(controlledFrame, callBack);

  const ORIGINAL_URL = getControlledFrameUrl(ORIGINAL_PATH);
  if (controlledFrame.src !== ORIGINAL_URL) {
    return `ERROR: back() failed to return page to ${ORIGINAL_URL}`;
  }

  const callPromiseForward = async function() {
    await controlledFrame.forward();
  };
  await runCallbackAndWaitForLoadStop(controlledFrame, callPromiseForward);
  if (controlledFrame.src !== TEST_PAGE_1_URL) {
    return `ERROR: forward() failed to return to page ${TEST_PAGE_1_PATH}`
  }

  // Go back again to test the non-promise version of the API.
  await runCallbackAndWaitForLoadStop(controlledFrame, callBack);
  if (controlledFrame.src !== ORIGINAL_URL) {
    return `ERROR: back(callback) failed to return page to ${ORIGINAL_URL}`;
  }

  return 'SUCCESS';
}

async function testGo(controlledFrame) {
  // Navigate twice to test go with indices greater than 1.
  await navigateFrame(controlledFrame, TEST_PAGE_1_PATH);
  const TEST_PAGE_1_URL = getControlledFrameUrl(TEST_PAGE_1_PATH);
  if (controlledFrame.src !== TEST_PAGE_1_URL) {
    return `ERROR: navigateFrame() failed to navigate to ${TEST_PAGE_1_URL}`;
  }

  await navigateFrame(controlledFrame, TEST_PAGE_2_PATH);
  const TEST_PAGE_2_URL = getControlledFrameUrl(TEST_PAGE_2_PATH);
  if (controlledFrame.src !== TEST_PAGE_2_URL) {
    return `ERROR: navigateFrame() failed to navigate to ${TEST_PAGE_2_URL}`;
  }

  // Go back to the first page.
  const goPromiseCallback = async function(index) {
    return await controlledFrame.go(index);
  };
  const ORIGINAL_URL = getControlledFrameUrl(ORIGINAL_PATH);
  await runCallbackAndWaitForLoadStop(
      controlledFrame, goPromiseCallback.bind(this, -2));
  if (controlledFrame.src !== ORIGINAL_URL) {
    return `ERROR: go() failed to navigate back to ${ORIGINAL_URL}`;
  }

  // Go forward to the last navigated page.
  await runCallbackAndWaitForLoadStop(
      controlledFrame, goPromiseCallback.bind(this, 2));
  if (controlledFrame.src !== TEST_PAGE_2_URL) {
    return `ERROR: go() failed to navigate forward to ${TEST_PAGE_2_URL}`;
  }

  return 'SUCCESS';
}
