// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTimeFormat, InterventionsInternalPageImpl, URL_THRESHOLD} from 'chrome://interventions-internals/index.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {assertEquals, assertFalse, assertGT} from './chai_assert.js';
import {TestBrowserProxy} from './test_browser_proxy.m.js';

/** @return {!PromiseResolver} */
function getSetupFnResolver() {
  return /** @type {{setupFnResolver: !PromiseResolver}} */ (window)
      .setupFnResolver;
}

suite('InterventionsInternalsUITest', function() {
  /**
   * A stub class for the Mojo PageHandler.
   * @implements {mojom.InterventionsInternalsPageInterface}
   * @implements {mojom.InterventionsInternalsPageHandlerInterface}
   */
  class TestPageHandler extends TestBrowserProxy {
    constructor() {
      super(['getPreviewsEnabled', 'getPreviewsFlagsDetails']);

      /** @private {!Array<!mojom.PreviewsStatus>} */
      this.previewsModeStatuses_ = [];

      /** @private {!Array<!mojom.PreviewsFlag>} */
      this.previewsFlagsStatuses_ = [];
    }

    /**
     * Setup testing map for getPreviewsEnabled.
     * @param {!Array<!mojom.PreviewsStatus>} map The testing status map.
     */
    setTestingPreviewsModeMap(map) {
      this.previewsModeStatuses_ = map;
    }

    /** @param {!Array<!mojom.PreviewsFlag>} map */
    setTestingPreviewsFlagsMap(map) {
      this.previewsFlagsStatuses_ = map;
    }

    /** @override **/
    getPreviewsEnabled() {
      this.methodCalled('getPreviewsEnabled');
      return Promise.resolve({
        statuses: this.previewsModeStatuses_,
      });
    }

    /** @override **/
    getPreviewsFlagsDetails() {
      this.methodCalled('getPreviewsFlagsDetails');
      return Promise.resolve({
        flags: this.previewsFlagsStatuses_,
      });
    }

    /** @override **/
    logNewMessage() {}
    /** @override **/
    onBlocklistCleared() {}
    /** @override **/
    onBlocklistedHost() {}
    /** @override **/
    onIgnoreBlocklistDecisionStatusChanged() {}
    /** @override **/
    onUserBlocklistedStatusChange() {}
    /** @override **/
    setClientPage() {}
    /** @override **/
    setIgnorePreviewsBlocklistDecision() {}
    /** @override **/
    updateEffectiveConnectionType() {}
  }

  function getBlocklistedStatus(blocklisted) {
    return (blocklisted ? 'Blocklisted' : 'Not blocklisted');
  }

  /** @type {?TestPageHandler} */
  let testPageHandler = null;

  setup(function() {
    testPageHandler = new TestPageHandler();
    window.testPageHandler = testPageHandler;
  });

  test('GetPreviewsEnabled', () => {
    // Setup testPageHandler behavior.
    let testArray = [
      {
        htmlId: 'params1',
        description: 'Params 1',
        enabled: true,
      },
      {
        htmlId: 'params2',
        description: 'Params 2',
        enabled: false,
      },
      {
        htmlId: 'params3',
        description: 'Param 3',
        enabled: false,
      },
    ];

    testPageHandler.setTestingPreviewsModeMap(testArray);
    getSetupFnResolver().resolve();

    return getSetupFnResolver()
        .promise
        .then(() => {
          return testPageHandler.whenCalled('getPreviewsEnabled');
        })
        .then(() => {
          testArray.forEach((value) => {
            let expected = value.description + ': ' +
                (value.enabled ? 'Enabled' : 'Disabled');
            let actual = document.querySelector('#' + value.htmlId).textContent;
            assertEquals(expected, actual);
          });
        });
  });

  test('GetPreviewsFlagsDetails', () => {
    // Setup testPageHandler behavior.
    let testArray = [
      {
        htmlId: 'params2',
        description: 'Params 2',
        link: 'Link 2',
        value: 'Value 2',
      },
      {
        htmlId: 'params3',
        description: 'Param 3',
        link: 'Link 3',
        value: 'Value 3',
      },
      {
        htmlId: 'params1',
        description: 'Params 1',
        link: 'Link 1',
        value: 'Value 1',
      },
    ];

    testPageHandler.setTestingPreviewsFlagsMap(testArray);
    getSetupFnResolver().resolve();

    return getSetupFnResolver()
        .promise
        .then(() => {
          return testPageHandler.whenCalled('getPreviewsFlagsDetails');
        })
        .then(() => {
          testArray.forEach((value) => {
            let key = value.htmlId;
            let actualDescription =
                document.querySelector('#' + key + 'Description');
            let actualValue = document.querySelector('#' + key + 'Value');
            assertEquals(value.description, actualDescription.textContent);
            assertEquals(value.link, actualDescription.getAttribute('href'));
            assertEquals(value.value, actualValue.textContent);
          });
        });
  });

  test('LogNewMessage', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 1507221689240,  // Oct 05 2017 16:41:29 UTC
        pageId: 0,
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 758675653000,  // Jan 15 1994 23:14:13 UTC
        pageId: 0,
      },
      {
        type: 'Type_c',
        description: 'Some description_c',
        url: {url: 'Some gurl.spec()_c'},
        time: -314307870000,  // Jan 16 1960 04:15:30 UTC
        pageId: 0,
      },
    ];

    logs.forEach((log) => {
      pageImpl.logNewMessage(log);
    });

    let rows = $('message-logs-table').querySelectorAll('.log-message');
    assertEquals(logs.length, rows.length);

    logs.forEach((log, index) => {
      let row = rows[logs.length - index - 1];  // Expecting reversed order.
                                                // (i.e. a new log message is
                                                // appended to the top of the
                                                // log table).

      assertEquals(
          getTimeFormat(log.time), row.querySelector('.log-time').textContent);
      assertEquals(log.type, row.querySelector('.log-type').textContent);
      assertEquals(
          log.description, row.querySelector('.log-description').textContent);
      assertEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  test('LogNewMessageWithLongUrl', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let log = /** @type {!mojom.MessageLog} */ ({
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      pageId: 0,
    });
    // Creating long url.
    for (let i = 0; i <= 2 * URL_THRESHOLD; i++) {
      log.url.url += 'a';
    }
    let expectedUrl = log.url.url.substring(0, URL_THRESHOLD - 3) + '...';

    pageImpl.logNewMessage(log);
    assertEquals(
        expectedUrl, document.querySelector('div.log-url-value').textContent);
  });


  test('LogNewMessageWithNoUrl', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let log = /** @type {!mojom.MessageLog} */ ({
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      pageId: 0,
    });
    pageImpl.logNewMessage(log);
    let actual = $('message-logs-table').rows[1];
    let expectedNoColumns = 3;
    assertEquals(expectedNoColumns, actual.querySelectorAll('td').length);
    assertFalse(
        !!actual.querySelector('.log-url'),
        'There should not be a log-url column for empty URL');
  });

  test('LogNewMessagePageIdZero', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 1507221689240,  // Oct 05 2017 16:41:29 UTC
        pageId: 0,
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 758675653000,  // Jan 15 1994 23:14:13 UTC
        pageId: 0,
      },
    ];

    logs.forEach((log) => {
      pageImpl.logNewMessage(log);
    });

    // Expect 2 different rows in logs table.
    let rows = $('message-logs-table').querySelectorAll('.log-message');
    let expectedNumberOfRows = 2;
    assertEquals(expectedNumberOfRows, rows.length);

    logs.forEach((log, index) => {
      let expectedTime = getTimeFormat(log.time);
      let row = rows[logs.length - index - 1];

      assertEquals(expectedTime, row.querySelector('.log-time').textContent);
      assertEquals(log.type, row.querySelector('.log-type').textContent);
      assertEquals(
          log.description, row.querySelector('.log-description').textContent);
      assertEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  test('LogNewMessageNewPageId', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 1507221689240,  // Oct 05 2017 16:41:29 UTC
        pageId: 123,
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 758675653000,  // Jan 15 1994 23:14:13 UTC
        pageId: 321,
      },
    ];

    logs.forEach((log) => {
      pageImpl.logNewMessage(log);
    });

    // Expect 2 different rows in logs table.
    let rows = $('message-logs-table').querySelectorAll('.log-message');
    assertEquals(2, rows.length);

    logs.forEach((log, index) => {
      let expectedTime = getTimeFormat(log.time);
      let row = rows[logs.length - index - 1];

      assertEquals(expectedTime, row.querySelector('.log-time').textContent);
      assertEquals(log.type, row.querySelector('.log-type').textContent);
      assertEquals(
          log.description, row.querySelector('.log-description').textContent);
      assertEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  test('LogNewMessageExistedPageId', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 1507221689240,  // Oct 05 2017 16:41:29 UTC
        pageId: 3,
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 758675653000,  // Jan 15 1994 23:14:13 UTC
        pageId: 3,
      },
    ];

    logs.forEach((log) => {
      pageImpl.logNewMessage(log);
    });

    let logTableRows = $('message-logs-table').querySelectorAll('.log-message');
    assertEquals(1, logTableRows.length);
    assertEquals(
        1, document.querySelector('.expansion-logs-table').rows.length);

    // Log table row.
    let row = $('message-logs-table').querySelector('.log-message');
    let expectedRowTime = getTimeFormat(logs[1].time);
    assertEquals(expectedRowTime, row.querySelector('.log-time').textContent);
    assertEquals(logs[1].type, row.querySelector('.log-type').textContent);
    assertEquals(
        logs[1].description, row.querySelector('.log-description').textContent);
    assertEquals(
        logs[1].url.url, row.querySelector('.log-url-value').textContent);

    // Sub log table row.
    let subRow = document.querySelector('.expansion-logs-table').rows[0];
    let expectedSubTime = getTimeFormat(logs[0].time);
    assertEquals(
        expectedSubTime, subRow.querySelector('.log-time').textContent);
    assertEquals(logs[0].type, subRow.querySelector('.log-type').textContent);
    assertEquals(
        logs[0].description,
        subRow.querySelector('.log-description').textContent);
    assertEquals(
        logs[0].url.url, subRow.querySelector('.log-url-value').textContent);
  });

  test('LogNewMessageExistedPageIdGroupToTopOfTable', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let logs = [
      {
        type: 'Type_a',
        description: 'Some description_a',
        url: {url: 'Some gurl.spec()_a'},
        time: 0,
        pageId: 3,
      },
      {
        type: 'Type_b',
        description: 'Some description_b',
        url: {url: 'Some gurl.spec()_b'},
        time: 1,
        pageId: 123,
      },
      {
        type: 'Type_c',
        description: 'Some description_c',
        url: {url: 'Some gurl.spec()_c'},
        time: 2,
        pageId: 3,
      },
    ];

    pageImpl.logNewMessage(logs[0]);
    pageImpl.logNewMessage(logs[1]);
    let rows = $('message-logs-table').querySelectorAll('.log-message');
    assertEquals(2, rows.length);
    assertEquals(logs[1].type, rows[0].querySelector('.log-type').textContent);
    assertEquals(logs[0].type, rows[1].querySelector('.log-type').textContent);

    // Existing group pushed to the top of the log table.
    pageImpl.logNewMessage(logs[2]);
    rows = $('message-logs-table').querySelectorAll('.log-message');
    assertEquals(2, rows.length);
    assertEquals(logs[2].type, rows[0].querySelector('.log-type').textContent);
    assertEquals(logs[1].type, rows[1].querySelector('.log-type').textContent);
  });

  test('AddNewBlocklistedHost', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC
    let expectedHost = 'example.com';
    pageImpl.onBlocklistedHost(expectedHost, BigInt(time));

    let blocklistedTable = $('blocklisted-hosts-table');
    let row = blocklistedTable.querySelector('.blocklisted-host-row');

    let expectedTime = getTimeFormat(time);

    assertEquals(
        expectedHost, row.querySelector('.host-blocklisted').textContent);
    assertEquals(
        expectedTime, row.querySelector('.host-blocklisted-time').textContent);
  });


  test('HostAlreadyBlocklisted', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let time0 = 758675653000;   // Jan 15 1994 23:14:13 UTC
    let time1 = 1507221689240;  // Oct 05 2017 16:41:29 UTC
    let expectedHost = 'example.com';

    pageImpl.onBlocklistedHost(expectedHost, BigInt(time0));

    let blocklistedTable = $('blocklisted-hosts-table');
    let row = blocklistedTable.querySelector('.blocklisted-host-row');
    let expectedTime = getTimeFormat(time0);
    assertEquals(
        expectedHost, row.querySelector('.host-blocklisted').textContent);
    assertEquals(
        expectedTime, row.querySelector('.host-blocklisted-time').textContent);

    pageImpl.onBlocklistedHost(expectedHost, BigInt(time1));

    // The row remains the same.
    assertEquals(
        expectedHost, row.querySelector('.host-blocklisted').textContent);
    assertEquals(
        expectedTime, row.querySelector('.host-blocklisted-time').textContent);
  });

  test('UpdateUserBlocklisted', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let state = $('user-blocklisted-status-value');

    pageImpl.onUserBlocklistedStatusChange(true /* blocklisted */);
    assertEquals(
        getBlocklistedStatus(true /* blocklisted */), state.textContent);

    pageImpl.onUserBlocklistedStatusChange(false /* blocklisted */);
    assertEquals(
        getBlocklistedStatus(false /* blocklisted */), state.textContent);
  });

  test('OnBlocklistCleared', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let state = $('user-blocklisted-status-value');
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC

    pageImpl.onBlocklistCleared(BigInt(time));
    let actualClearedTime = $('blocklist-last-cleared-time').textContent;
    assertEquals(getTimeFormat(time), actualClearedTime);
    let blocklistedTable = $('blocklisted-hosts-table');
    let rows = blocklistedTable.querySelectorAll('.blocklisted-host-row');
    assertEquals(0, rows.length);
  });

  test('ClearLogMessageOnBlocklistCleared', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC
    let log = /** @type {!mojom.MessageLog} */ ({
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      pageId: 0,
    });

    pageImpl.logNewMessage(log);
    assertGT($('message-logs-table').rows.length, 1 /* header row */);
    pageImpl.onBlocklistCleared(BigInt(time));
    let expectedNumberOfRows = 2;  // header row and clear blocklist log.
    let rows = $('message-logs-table').rows;
    assertEquals(expectedNumberOfRows, rows.length);
    assertEquals(
        'Blocklist Cleared',
        rows[1].querySelector('.log-description').textContent);
  });


  test('OnECTChanged', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    let ectTypes = ['type1', 'type2', 'type3'];
    ectTypes.forEach((type) => {
      pageImpl.updateEffectiveConnectionType(type, 'max');
      let actual = $('nqe-type').textContent;
      assertEquals(type, actual);
    });
  });

  test('OnBlocklistIgnoreChange', () => {
    let pageImpl = new InterventionsInternalPageImpl();
    pageImpl.onIgnoreBlocklistDecisionStatusChanged(true /* ignored */);
    assertEquals('Enable Blocklist', $('ignore-blocklist-button').textContent);
    assertEquals(
        'Blocklist decisions are ignored.',
        $('blocklist-ignored-status').textContent);

    pageImpl.onIgnoreBlocklistDecisionStatusChanged(false /* ignored */);
    assertEquals('Ignore Blocklist', $('ignore-blocklist-button').textContent);
    assertEquals('', $('blocklist-ignored-status').textContent);
  });
});
