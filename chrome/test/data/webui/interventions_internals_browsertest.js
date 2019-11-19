// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for interventions_internals.js
 */

/**
 * Test fixture for InterventionsInternals WebUI testing.
 * @constructor
 * @extends testing.Test
 */
function InterventionsInternalsUITest() {
  this.setupFnResolver = new PromiseResolver();
}

InterventionsInternalsUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the options page and call preLoad().
   * @override
   */
  browsePreload: 'chrome://interventions-internals',

  /** @override */
  isAsync: true,

  extraLibraries: [
    '//third_party/mocha/mocha.js',
    '//chrome/test/data/webui/mocha_adapter.js',
    '//ui/webui/resources/js/promise_resolver.js',
    '//ui/webui/resources/js/util.js',
    '//chrome/test/data/webui/test_browser_proxy.js',
  ],

  preLoad: function() {
    /**
     * A stub class for the Mojo PageHandler.
     */
    class TestPageHandler extends TestBrowserProxy {
      constructor() {
        super(['getPreviewsEnabled', 'getPreviewsFlagsDetails']);

        /** @private {!Map} */
        this.previewsModeStatuses_ = new Map();
        this.previewsFlagsStatuses_ = new Map();
      }

      /**
       * Setup testing map for getPreviewsEnabled.
       * @param {!Map} map The testing status map.
       */
      setTestingPreviewsModeMap(map) {
        this.previewsModeStatuses_ = map;
      }

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
    }

    window.setupFn = function() {
      return this.setupFnResolver.promise;
    }.bind(this);

    window.testPageHandler = new TestPageHandler();

    getBlacklistedStatus = function(blacklisted) {
      return (blacklisted ? 'Blacklisted' : 'Not blacklisted');
    };
  },
};

TEST_F('InterventionsInternalsUITest', 'GetPreviewsEnabled', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisplayCorrectStatuses', () => {
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

    window.testPageHandler.setTestingPreviewsModeMap(testArray);
    this.setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(() => {
          return window.testPageHandler.whenCalled('getPreviewsEnabled');
        })
        .then(() => {
          testArray.forEach((value) => {
            let expected = value.description + ': ' +
                (value.enabled ? 'Enabled' : 'Disabled');
            let actual = document.querySelector('#' + value.htmlId).textContent;
            expectEquals(expected, actual);
          });
        });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'GetPreviewsFlagsDetails', function() {
  let setupFnResolver = this.setupFnResolver;

  test('DisplayCorrectStatuses', () => {
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

    window.testPageHandler.setTestingPreviewsFlagsMap(testArray);
    this.setupFnResolver.resolve();

    return setupFnResolver.promise
        .then(() => {
          return window.testPageHandler.whenCalled('getPreviewsFlagsDetails');
        })
        .then(() => {
          testArray.forEach((value) => {
            let key = value.htmlId;
            let actualDescription =
                document.querySelector('#' + key + 'Description');
            let actualValue = document.querySelector('#' + key + 'Value');
            expectEquals(value.description, actualDescription.textContent);
            expectEquals(value.link, actualDescription.getAttribute('href'));
            expectEquals(value.value, actualValue.textContent);
          });
        });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessage', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
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
    expectEquals(logs.length, rows.length);

    logs.forEach((log, index) => {
      let row = rows[logs.length - index - 1];  // Expecting reversed order.
                                                // (i.e. a new log message is
                                                // appended to the top of the
                                                // log table).

      expectEquals(
          getTimeFormat(log.time), row.querySelector('.log-time').textContent);
      expectEquals(log.type, row.querySelector('.log-type').textContent);
      expectEquals(
          log.description, row.querySelector('.log-description').textContent);
      expectEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessageWithLongUrl', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let log = {
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      pageId: 0,
    };
    // Creating long url.
    for (let i = 0; i <= 2 * URL_THRESHOLD; i++) {
      log.url.url += 'a';
    }
    let expectedUrl = log.url.url.substring(0, URL_THRESHOLD - 3) + '...';

    pageImpl.logNewMessage(log);
    expectEquals(
        expectedUrl, document.querySelector('div.log-url-value').textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessageWithNoUrl', function() {
  test('LogMessageIsPostedCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let log = {
      type: 'Some type',
      url: {url: ''},
      description: 'Some description',
      time: 758675653000,  // Jan 15 1994 23:14:13 UTC
      pageId: 0,
    };
    pageImpl.logNewMessage(log);
    let actual = $('message-logs-table').rows[1];
    let expectedNoColumns = 3;
    expectEquals(expectedNoColumns, actual.querySelectorAll('td').length);
    assert(
        !actual.querySelector('.log-url'),
        'There should not be a log-url column for empty URL');
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessagePageIdZero', function() {
  test('LogMessageWithPageIdZero', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
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
    expectEquals(expectedNumberOfRows, rows.length);

    logs.forEach((log, index) => {
      let expectedTime = getTimeFormat(log.time);
      let row = rows[logs.length - index - 1];

      expectEquals(expectedTime, row.querySelector('.log-time').textContent);
      expectEquals(log.type, row.querySelector('.log-type').textContent);
      expectEquals(
          log.description, row.querySelector('.log-description').textContent);
      expectEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'LogNewMessageNewPageId', function() {
  test('LogMessageWithNewPageId', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
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
    expectEquals(2, rows.length);

    logs.forEach((log, index) => {
      let expectedTime = getTimeFormat(log.time);
      let row = rows[logs.length - index - 1];

      expectEquals(expectedTime, row.querySelector('.log-time').textContent);
      expectEquals(log.type, row.querySelector('.log-type').textContent);
      expectEquals(
          log.description, row.querySelector('.log-description').textContent);
      expectEquals(
          log.url.url, row.querySelector('.log-url-value').textContent);
    });
  });

  mocha.run();
});

TEST_F(
    'InterventionsInternalsUITest', 'LogNewMessageExistedPageId', function() {
      test('LogMessageWithExistedPageId', () => {
        let pageImpl = new InterventionsInternalPageImpl(null);
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

        let logTableRows =
            $('message-logs-table').querySelectorAll('.log-message');
        expectEquals(1, logTableRows.length);
        expectEquals(
            1, document.querySelector('.expansion-logs-table').rows.length);

        // Log table row.
        let row = $('message-logs-table').querySelector('.log-message');
        let expectedRowTime = getTimeFormat(logs[1].time);
        expectEquals(
            expectedRowTime, row.querySelector('.log-time').textContent);
        expectEquals(logs[1].type, row.querySelector('.log-type').textContent);
        expectEquals(
            logs[1].description,
            row.querySelector('.log-description').textContent);
        expectEquals(
            logs[1].url.url, row.querySelector('.log-url-value').textContent);

        // Sub log table row.
        let subRow = document.querySelector('.expansion-logs-table').rows[0];
        let expectedSubTime = getTimeFormat(logs[0].time);
        expectEquals(
            expectedSubTime, subRow.querySelector('.log-time').textContent);
        expectEquals(
            logs[0].type, subRow.querySelector('.log-type').textContent);
        expectEquals(
            logs[0].description,
            subRow.querySelector('.log-description').textContent);
        expectEquals(
            logs[0].url.url,
            subRow.querySelector('.log-url-value').textContent);
      });

      mocha.run();
    });

TEST_F(
    'InterventionsInternalsUITest',
    'LogNewMessageExistedPageIdGroupToTopOfTable', function() {
      test('NewMessagePushedToTopOfTable', () => {
        let pageImpl = new InterventionsInternalPageImpl(null);
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
        expectEquals(2, rows.length);
        expectEquals(
            logs[1].type, rows[0].querySelector('.log-type').textContent);
        expectEquals(
            logs[0].type, rows[1].querySelector('.log-type').textContent);

        // Existing group pushed to the top of the log table.
        pageImpl.logNewMessage(logs[2]);
        rows = $('message-logs-table').querySelectorAll('.log-message');
        expectEquals(2, rows.length);
        expectEquals(
            logs[2].type, rows[0].querySelector('.log-type').textContent);
        expectEquals(
            logs[1].type, rows[1].querySelector('.log-type').textContent);
      });

      mocha.run();
    });

TEST_F('InterventionsInternalsUITest', 'AddNewBlacklistedHost', function() {
  test('AddNewBlacklistedHost', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC
    let expectedHost = 'example.com';
    pageImpl.onBlacklistedHost(expectedHost, time);

    let blacklistedTable = $('blacklisted-hosts-table');
    let row = blacklistedTable.querySelector('.blacklisted-host-row');

    let expectedTime = getTimeFormat(time);

    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'HostAlreadyBlacklisted', function() {
  test('HostAlreadyBlacklisted', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let time0 = 758675653000;   // Jan 15 1994 23:14:13 UTC
    let time1 = 1507221689240;  // Oct 05 2017 16:41:29 UTC
    let expectedHost = 'example.com';

    pageImpl.onBlacklistedHost(expectedHost, time0);

    let blacklistedTable = $('blacklisted-hosts-table');
    let row = blacklistedTable.querySelector('.blacklisted-host-row');
    let expectedTime = getTimeFormat(time0);
    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);

    pageImpl.onBlacklistedHost(expectedHost, time1);

    // The row remains the same.
    expectEquals(
        expectedHost, row.querySelector('.host-blacklisted').textContent);
    expectEquals(
        expectedTime, row.querySelector('.host-blacklisted-time').textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'UpdateUserBlacklisted', function() {
  test('UpdateUserBlacklistedDisplayCorrectly', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let state = $('user-blacklisted-status-value');

    pageImpl.onUserBlacklistedStatusChange(true /* blacklisted */);
    expectEquals(
        getBlacklistedStatus(true /* blacklisted */), state.textContent);

    pageImpl.onUserBlacklistedStatusChange(false /* blacklisted */);
    expectEquals(
        getBlacklistedStatus(false /* blacklisted */), state.textContent);
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'OnBlacklistCleared', function() {
  test('OnBlacklistClearedRemovesAllBlacklistedHostInTable', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let state = $('user-blacklisted-status-value');
    let time = 758675653000;  // Jan 15 1994 23:14:13 UTC

    pageImpl.onBlacklistCleared(time);
    let actualClearedTime = $('blacklist-last-cleared-time').textContent;
    expectEquals(getTimeFormat(time), actualClearedTime);
    let blacklistedTable = $('blacklisted-hosts-table');
    let rows = blacklistedTable.querySelectorAll('.blacklisted-host-row');
    expectEquals(0, rows.length);
  });

  mocha.run();
});

TEST_F(
    'InterventionsInternalsUITest', 'ClearLogMessageOnBlacklistCleared',
    function() {
      test('ClearLogsTableOnBlacklistCleared', () => {
        let pageImpl = new InterventionsInternalPageImpl(null);
        let time = 758675653000;  // Jan 15 1994 23:14:13 UTC
        let log = {
          type: 'Some type',
          url: {url: ''},
          description: 'Some description',
          time: 758675653000,  // Jan 15 1994 23:14:13 UTC
          pageId: 0,
        };

        pageImpl.logNewMessage(log);
        expectGT($('message-logs-table').rows.length, 1 /* header row */);
        pageImpl.onBlacklistCleared(time);
        let expectedNumberOfRows = 2;  // header row and clear blacklist log.
        let rows = $('message-logs-table').rows;
        expectEquals(expectedNumberOfRows, rows.length);
        expectEquals(
            'Blacklist Cleared',
            rows[1].querySelector('.log-description').textContent);
      });

      mocha.run();
    });

TEST_F('InterventionsInternalsUITest', 'OnECTChanged', function() {
  test('UpdateETCOnChange', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    let ectTypes = ['type1', 'type2', 'type3'];
    ectTypes.forEach((type) => {
      pageImpl.updateEffectiveConnectionType(type, 'max');
      let actual = $('nqe-type').textContent;
      expectEquals(type, actual);
    });
  });

  mocha.run();
});

TEST_F('InterventionsInternalsUITest', 'OnBlacklistIgnoreChange', function() {
  test('OnBlacklistIgnoreChangeDisable', () => {
    let pageImpl = new InterventionsInternalPageImpl(null);
    pageImpl.onIgnoreBlacklistDecisionStatusChanged(true /* ignored */);
    expectEquals('Enable Blacklist', $('ignore-blacklist-button').textContent);
    expectEquals(
        'Blacklist decisions are ignored.',
        $('blacklist-ignored-status').textContent);

    pageImpl.onIgnoreBlacklistDecisionStatusChanged(false /* ignored */);
    expectEquals('Ignore Blacklist', $('ignore-blacklist-button').textContent);
    expectEquals('', $('blacklist-ignored-status').textContent);
  });

  mocha.run();
});
