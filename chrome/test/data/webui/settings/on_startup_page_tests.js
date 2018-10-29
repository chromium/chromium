// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.ProfileInfoBrowserProxy} */
class TestOnStartupBrowserProxy extends TestBrowserProxy {
  constructor() {
    super(['getNtpExtension']);

    /** @private {?NtpExtension} */
    this.ntpExtension_ = null;
  }

  /** @override */
  getNtpExtension() {
    this.methodCalled('getNtpExtension', arguments);
    return Promise.resolve(this.ntpExtension_);
  }

  /**
   * Sets ntpExtension and fires an update event
   * @param {?NtpExtension}
   */
  setNtpExtension(ntpExtension) {
    this.ntpExtension_ = ntpExtension;
    cr.webUIListenerCallback('update-ntp-extension', ntpExtension);
  }
}

/** @fileoverview Suite of tests for on_startup_page. */
suite('OnStartupPage', function() {
  /**
   * Radio button enum values for restore on startup.
   * @enum
   */
  const RestoreOnStartupEnum = {
    CONTINUE: 1,
    OPEN_NEW_TAB: 5,
    OPEN_SPECIFIC: 4,
  };

  let testElement;

  /**
   * The mock proxy object to use during test.
   * @type {TestOnStartupBrowserProxy}
   */
  let onStartupBrowserProxy = null;

  /** @type {NtpExtension} */
  const ntpExtension = {id: 'id', name: 'name', canBeDisabled: true};

  /** @return {!Promise} */
  function initPage() {
    onStartupBrowserProxy.reset();
    PolymerTest.clearBody();
    testElement = document.createElement('settings-on-startup-page');
    testElement.prefs = {
      session: {
        restore_on_startup: {
          type: chrome.settingsPrivate.PrefType.NUMBER,
          value: RestoreOnStartupEnum.OPEN_NEW_TAB,
        },
      },
    };
    document.body.appendChild(testElement);
    return onStartupBrowserProxy.whenCalled('getNtpExtension').then(function() {
      Polymer.dom.flush();
    });
  }

  function getSelectedOptionLabel() {
    return Array
        .from(testElement.root.querySelectorAll('controlled-radio-button'))
        .find(el => el.name == testElement.$.onStartupRadioGroup.selected)
        .label;
  }

  setup(function() {
    onStartupBrowserProxy = new TestOnStartupBrowserProxy();
    settings.OnStartupBrowserProxyImpl.instance_ = onStartupBrowserProxy;
    return initPage();
  });

  teardown(function() {
    if (testElement) {
      testElement.remove();
      testElement = null;
    }
  });

  test('open-continue', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.CONTINUE);
    assertEquals('Continue where you left off', getSelectedOptionLabel());
  });

  test('open-ntp', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.OPEN_NEW_TAB);
    assertEquals('Open the New Tab page', getSelectedOptionLabel());
  });

  test('open-specific', function() {
    testElement.set(
        'prefs.session.restore_on_startup.value',
        RestoreOnStartupEnum.OPEN_SPECIFIC);
    assertEquals(
        'Open a specific page or set of pages', getSelectedOptionLabel());
  });

  function extensionControlledIndicatorExists() {
    return !!testElement.$$('extension-controlled-indicator');
  }

  test('given ntp extension, extension indicator always exists', function() {
    onStartupBrowserProxy.setNtpExtension(ntpExtension);
    return onStartupBrowserProxy.whenCalled('getNtpExtension').then(function() {
      Polymer.dom.flush();
      assertTrue(extensionControlledIndicatorExists());
      Object.values(RestoreOnStartupEnum).forEach(function(option) {
        testElement.set('prefs.session.restore_on_startup.value', option);
        assertTrue(extensionControlledIndicatorExists());
      });
    });
  });

  test(
      'extension indicator not shown when no ntp extension enabled',
      function() {
        assertFalse(extensionControlledIndicatorExists());
        Object.values(RestoreOnStartupEnum).forEach(function(option) {
          testElement.set('prefs.session.restore_on_startup.value', option);
          assertFalse(extensionControlledIndicatorExists());
        });
      });

  test('ntp extension updated, extension indicator added', function() {
    assertFalse(extensionControlledIndicatorExists());
    onStartupBrowserProxy.setNtpExtension(ntpExtension);
    return onStartupBrowserProxy.whenCalled('getNtpExtension').then(function() {
      Polymer.dom.flush();
      assertTrue(extensionControlledIndicatorExists());
    });
  });
});
