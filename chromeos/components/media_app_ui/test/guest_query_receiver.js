// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The last file list loaded into the guest, updated via a spy on loadFiles().
 * @type {?ReceivedFileList}
 */
let lastReceivedFileList = null;

/**
 * Test cases registered by GUEST_TEST.
 * @type {!Map<string, function(): !Promise<undefined>>}
 */
const guestTestCases = new Map();

/**
 * @return {!mediaApp.AbstractFile}
 */
function firstReceivedItem() {
  return assertCast(assertCast(lastReceivedFileList).item(0));
}

/**
 * Acts on received TestMessageQueryData.
 * @param {!TestMessageQueryData} data
 * @return {!Promise<!TestMessageResponseData>}
 */
async function runTestQuery(data) {
  let result = 'no result';
  let extraResultData;
  if (data.testQuery) {
    const element = await waitForNode(data.testQuery, data.pathToRoot || []);
    result = element.tagName;

    if (data.property) {
      result = JSON.stringify(element[data.property]);
    } else if (data.requestFullscreen) {
      try {
        await element.requestFullscreen();
        result = 'hooray';
      } catch (/** @type {!TypeError} */ typeError) {
        result = typeError.message;
      }
    }
  } else if (data.navigate !== undefined) {
    if (data.navigate.direction === 'next') {
      await assertCast(lastReceivedFileList).loadNext(data.navigate.token);
      result = 'loadNext called';
    } else if (data.navigate.direction === 'prev') {
      await assertCast(lastReceivedFileList).loadPrev(data.navigate.token);
      result = 'loadPrev called';
    } else {
      result = 'nothing called';
    }
  } else if (data.overwriteLastFile) {
    const testBlob = new Blob([data.overwriteLastFile]);
    const file = firstReceivedItem();
    await assertCast(file.overwriteOriginal).call(file, testBlob);
    extraResultData = {
      receiverFileName: file.name,
      receiverErrorName: file.error
    };
    result = 'overwriteOriginal resolved';
  } else if (data.deleteLastFile) {
    try {
      const deleteResult =
          await assertCast(firstReceivedItem().deleteOriginalFile)
              .call(firstReceivedItem());
      if (deleteResult === DeleteResult.FILE_MOVED) {
        result = 'deleteOriginalFile resolved file moved';
      } else {
        result = 'deleteOriginalFile resolved success';
      }
    } catch (/** @type{!Error} */ error) {
      result = `deleteOriginalFile failed Error: ${error}`;
    }
  } else if (data.renameLastFile) {
    try {
      const renameResult =
          await assertCast(firstReceivedItem().renameOriginalFile)
              .call(firstReceivedItem(), data.renameLastFile);
      if (renameResult === RenameResult.FILE_EXISTS) {
        result = 'renameOriginalFile resolved file exists';
      } else if (
          renameResult ===
          RenameResult.FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY) {
        result = 'renameOriginalFile resolved ' +
            'FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY';
      } else {
        result = 'renameOriginalFile resolved success';
      }
    } catch (/** @type{!Error} */ error) {
      result = `renameOriginalFile failed Error: ${error}`;
    }
  } else if (data.requestSaveFile) {
    const existingFile = assertCast(lastReceivedFileList).item(0);
    if (!existingFile) {
      result = 'requestSaveFile failed, no file loaded';
    } else {
      const pickedFile = await DELEGATE.requestSaveFile(
          existingFile.name, existingFile.mimeType);
      result = assertCast(pickedFile.token).toString();
    }
  } else if (data.saveAs) {
    const existingFile = assertCast(lastReceivedFileList).item(0);
    if (!existingFile) {
      result = 'saveAs failed, no file loaded';
    } else {
      const file = firstReceivedItem();
      try {
        const token = (await DELEGATE.requestSaveFile(
                           existingFile.name, existingFile.mimeType))
                          .token;
        const testBlob = new Blob([data.saveAs]);
        await assertCast(file.saveAs).call(file, testBlob, assertCast(token));
        result = file.name;
        extraResultData = {blobText: await file.blob.text()};
      } catch (/** @type{!Error} */ error) {
        result = `saveAs failed Error: ${error}`;
        extraResultData = {filename: file.name};
      }
    }
  } else if (data.getFileErrors) {
    result =
        assertCast(lastReceivedFileList).files.map(file => file.error).join();
  } else if (data.openFile) {
    await DELEGATE.openFile();
  } else if (data.getLastFileName) {
    result = firstReceivedItem().name;
  }
  return {testQueryResult: result, testQueryResultData: extraResultData};
}

/**
 * Acts on TestMessageRunTestCase.
 * @param {!TestMessageRunTestCase} data
 * @return {!Promise<!TestMessageResponseData>}
 */
async function runTestCase(data) {
  const testCase = guestTestCases.get(data.testCase);
  if (!testCase) {
    throw new Error(`Unknown test case: '${data.testCase}'`);
  }
  await testCase();  // Propate exceptions to the MessagePipe handler.
  return {testQueryResult: 'success'};
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test throws an exception (e.g. via assertEquals).
 * @param {string} testName
 * @param {function(): !Promise<undefined>} testCase
 */
function GUEST_TEST(testName, testCase) {
  guestTestCases.set(testName, testCase);
}

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in driver.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      `No handler registered for message type 'test-handlers-ready'`;
  while (true) {
    try {
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (/** @type {!GenericErrorResponse} */ e) {
      if (e.message !== EXPECTED_ERROR) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
}

/** Installs the MessagePipe handlers for receiving test queries. */
function installTestHandlers() {
  parentMessagePipe.registerHandler('test', (data) => {
    return runTestQuery(/** @type {!TestMessageQueryData} */ (data));
  });
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;

  parentMessagePipe.registerHandler('run-test-case', (data) => {
    return runTestCase(/** @type{!TestMessageRunTestCase} */ (data));
  });

  parentMessagePipe.registerHandler('get-last-loaded-files', () => {
    //  Note: the `ReceivedFileList` has methods stripped since it gets sent
    //  over a pipe so just send the underlying files.
    /**
     * @param {!mediaApp.AbstractFile} file
     * @return {!FileSnapshot}
     */
    function snapshot(file) {
      const hasDelete = !!file.deleteOriginalFile;
      const hasRename = !!file.renameOriginalFile;
      const {blob, name, size, mimeType, fromClipboard, error} = file;
      return {
        blob,
        name,
        size,
        mimeType,
        fromClipboard,
        error,
        hasDelete,
        hasRename
      };
    }
    return /** @type {!LastLoadedFilesResponse} */ (
        {fileList: assertCast(lastReceivedFileList).files.map(snapshot)});
  });

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly, and is also required for
  // signalTestHandlersReady() to operate without failing tests.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  // Install spies.
  const realLoadFiles = loadFiles;
  /**
   * @param {!ReceivedFileList} fileList
   * @return {!Promise<undefined>}
   */
  async function watchLoadFiles(fileList) {
    lastReceivedFileList = fileList;
    return realLoadFiles(fileList);
  }
  loadFiles = watchLoadFiles;
  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}

//# sourceURL=guest_query_receiver.js
