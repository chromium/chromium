// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for chrome/browser/resources/chromeos/arc_support/background.js

GEN('#include "content/public/test/browser_test.h"');

/**
 * @constructor
 * @extends testing.Test
 */
function ArcSupportTest() {}

ArcSupportTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: 'chrome://dummyurl/',

  /**
   * The mocha adapter assumes all tests are async.
   * @override
   * @final
   */
  isAsync: true,

  /**
   * Files that need not be compiled.
   * @override
   */
  extraLibraries: [
    'arc_support_test_setup.js',
    '//chrome/browser/resources/chromeos/' +
        'arc_support/i18n_template_no_process.js',
    '//ui/webui/resources/js/load_time_data_deprecated.js',
    '//chrome/browser/resources/chromeos/arc_support/background.js',
    '//third_party/node/node_modules/mocha/mocha.js',
    '//chrome/test/data/webui/chromeos/mocha_adapter.js',
  ],
};

TEST_F('ArcSupportTest', 'All', function() {
  suite('ArcSupportTest', function() {
    let closedListeners;
    let originalConsoleError;
    let recordedErrors;
    let webviewListeners;
    let termsView;

    function sleep(ms) {
      return new Promise((resolve) => setTimeout(resolve, ms));
    }

    function sendHostMessage(msg) {
      for (const listener of
               window.arcSupportTestHarness.nativeMessageListeners) {
        listener(msg);
      }
    }

    setup(function() {
      closedListeners = [];
      recordedErrors = [];
      originalConsoleError = console.error;
      console.error = function(...args) {
        if (args[0] && String(args[0]).startsWith('Mocha test failed:')) {
          originalConsoleError.apply(console, args);
          return;
        }
        recordedErrors.push(args.join(' '));
      };
      webviewListeners = {};
      window.loadTimeData.data_ = null;
      window.arcSupportTestHarness.sentMessages = [];
      window.arcSupportTestHarness.nativeMessageListeners = [];
      window.arcSupportTestHarness.disconnected = false;

      document.body.innerHTML = `
        <div id="terms" class="section" hidden>
          <div class="header"></div>
          <div id="terms-of-service-loading"></div>
          <div id="terms-of-service-content" hidden>
            <div id="terms-container"><webview id="terms-view"></webview></div>
            <div id="service-container">
              <div id="metrics-preference">
                <input type="checkbox" class="checkbox-option">
                <p class="checkbox-text"></p>
                <p class="content-text"></p>
              </div>
              <div id="backup-restore-preference">
                <input type="checkbox" class="checkbox-option">
                <p class="checkbox-text">
                  <a id="learn-more-link-backup-restore" href="#">Learn</a>
                </p>
              </div>
              <div id="location-service-preference">
                <input type="checkbox" class="checkbox-option">
                <p class="checkbox-text">
                  <a id="learn-more-link-location-service" href="#">Learn</a>
                </p>
              </div>
              <div id="pai-service-description">
                <p class="content-text">
                  <a id="learn-more-link-pai" href="#">Learn</a>
                </p>
              </div>
              <div id="google-service-confirmation"></div>
            </div>
            <button id="button-agree"></button>
            <button id="button-next"></button>
            <button id="button-cancel"></button>
          </div>
        </div>
        <div id="arc-loading" class="section" hidden></div>
        <div id="error" class="section" hidden>
          <div id="error-message"></div>
          <button id="button-send-feedback"></button>
          <div id="div-error-separating-buttons"></div>
          <button id="button-run-network-tests"></button>
          <button id="button-retry"></button>
        </div>
        <div id="overlay-container" hidden>
          <button id="overlay-close"></button>
          <div id="overlay-text-content"></div>
          <webview id="overlay-url"></webview>
        </div>
        <a id="privacy-policy-link" href="#">Privacy</a>
        <button id="close-button"></button>
      `;

      termsView = document.getElementById('terms-view');
      let currentSrc = '';
      termsView.srcAssignCount = 0;
      Object.defineProperty(termsView, 'src', {
        configurable: true,
        get() {
          return currentSrc;
        },
        set(value) {
          currentSrc = value;
          termsView.srcAssignCount++;
        },
      });
      termsView.stopCount = 0;
      termsView.reloadCount = 0;
      termsView.addContentScripts = function() {};
      termsView.executeScript = function(_details, callback) {
        if (termsView.scriptImpl) {
          termsView.scriptImpl(callback);
        }
      };
      termsView.stop = function() {
        termsView.stopCount++;
        if (webviewListeners['loadabort']) {
          webviewListeners['loadabort']({reason: 'ERR_ABORTED'});
        }
      };
      termsView.reload = function() {
        termsView.reloadCount++;
      };
      termsView.addEventListener = function(type, fn) {
        webviewListeners[type] = fn;
      };
      termsView.request = {
        onCompleted: {
          addListener() {},
        },
      };

      const overlayUrl = document.getElementById('overlay-url');
      overlayUrl.addContentScripts = function() {};

      window.cr = window.cr || {};
      window.cr.ui = window.cr.ui || {};
      window.cr.ui.ControlledIndicator = function() {
        return document.createElement('span');
      };
      window.cr.ui.overlay = {
        setupOverlay() {},
        globalInitialization() {},
      };
      window.ArcOptInFocusManager = function() {
        this.initialize = function() {};
      };
      window.startProgressAnimation = function() {};
      window.stopProgressAnimation = function() {};

      const fakeCreatedWindow = {
        contentWindow: window,
        outerBounds: {left: 0, top: 0, width: 768, height: 640},
        innerBounds: {width: 768, height: 640},
        show() {},
        close() {
          for (const fn of closedListeners) {
            fn();
          }
        },
        onClosed: {
          addListener(fn) {
            closedListeners.push(fn);
          },
        },
      };

      window.arcSupportTestHarness.createWindowImpl = function(callback) {
        callback(fakeCreatedWindow);
        fakeCreatedWindow.contentWindow.onload();
      };

      window.arcSupportTestHarness.launchedListener();
      sendHostMessage({
        action: 'initialize',
        deviceId: 'test-device',
        data: {
          locale: 'en',
          arcManaged: false,
          countryCode: 'us',
          isOwnerProfile: true,
          learnMoreStatistics: '',
          textMetricsDisabled: '',
          textMetricsEnabled: '',
          textMetricsManagedDisabled: '',
          textMetricsManagedEnabled: '',
          learnMoreBackupAndRestore: '',
          learnMoreLocationServices: '',
          learnMorePaiService: '',
          controlledByPolicy: '',
          overlayClose: 'Close',
          serverError: 'Server error',
        },
      });
    });

    teardown(function() {
      if (termsPage && termsPage.clearTermsViewLoadTimeout_) {
        termsPage.clearTermsViewLoadTimeout_();
      }
      console.error = originalConsoleError;
    });

    test('abortsAndAllowsRetryWhenLoadTimesOut', async function() {
      const {assertEquals, assertFalse, assertTrue} =
          await import('chrome://webui-test/chai_assert.js');

      termsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      assertEquals(1, termsView.srcAssignCount);
      assertEquals(LoadState.LOADING, termsPage.state_);
      webviewListeners['loadstart']();
      assertEquals(LoadState.LOADING, termsPage.state_);

      await sleep(90);

      assertEquals(LoadState.ABORTED, termsPage.state_);
      assertEquals(1, termsView.stopCount);
      assertTrue(document.getElementById('terms').hidden);
      assertFalse(document.getElementById('error').hidden);
      assertTrue(recordedErrors.some(
          (msg) => msg.includes('TermsView loading is aborted: timed out')));

      const loadResults = window.arcSupportTestHarness.sentMessages.filter(
          (m) => m.event === 'onTosLoadResult');
      assertEquals(1, loadResults.length);
      assertFalse(loadResults[0].success);

      // Retry after timeout must immediately transition back to LOADING and
      // reassign `termsView.src` rather than calling `reload()` (which is a
      // no-op in NavigationControllerImpl after `stop()` aborts an uncommitted
      // initial load).
      document.getElementById('button-retry').click();
      sendHostMessage({action: 'showPage', page: 'terms'});
      assertEquals(LoadState.LOADING, termsPage.state_);
      assertEquals(2, termsView.srcAssignCount);
      assertEquals(0, termsView.reloadCount);

      // If the retry also stalls before `loadstart` fires, the watchdog must
      // still abort the retry rather than returning early on a stale ABORTED
      // state.
      await sleep(90);
      assertEquals(LoadState.ABORTED, termsPage.state_);
      assertEquals(2, termsView.stopCount);
    });

    test('abortsOnTimeoutEvenBeforeLoadStartFires', async function() {
      const {assertEquals, assertFalse} =
          await import('chrome://webui-test/chai_assert.js');

      termsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      assertEquals(LoadState.LOADING, termsPage.state_);

      await sleep(90);

      assertEquals(LoadState.ABORTED, termsPage.state_);
      assertFalse(document.getElementById('error').hidden);
    });

    test('clearsTimeoutWhenTermsLoadSucceeds', async function() {
      const {assertEquals, assertFalse, assertTrue} =
          await import('chrome://webui-test/chai_assert.js');

      termsView.scriptImpl = (callback) => callback(['Play Terms Content']);
      termsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      webviewListeners['loadstart']();
      webviewListeners['contentload']();

      assertEquals(LoadState.LOADED, termsPage.state_);
      await sleep(90);

      assertEquals(LoadState.LOADED, termsPage.state_);
      assertFalse(document.getElementById('terms').hidden);
      assertTrue(document.getElementById('error').hidden);
      assertEquals(0, termsView.stopCount);
    });

    test('clearsTimeoutWhenWindowCloses', async function() {
      const {assertEquals} = await import('chrome://webui-test/chai_assert.js');

      termsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      webviewListeners['loadstart']();
      closeWindow();

      await sleep(90);
      assertEquals(0, termsView.stopCount);
    });

    test('stopsLoadingWhenShowErrorPageCalled', async function() {
      const {assertEquals} = await import('chrome://webui-test/chai_assert.js');

      termsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      assertEquals(LoadState.LOADING, termsPage.state_);

      sendHostMessage({
        action: 'showErrorPage',
        errorMessage: 'Error',
        shouldShowSendFeedback: false,
        shouldShowNetworkTests: false,
      });
      assertEquals(LoadState.ABORTED, termsPage.state_);
      assertEquals(1, termsView.stopCount);

      await sleep(90);
      assertEquals(1, termsView.stopCount);
    });

    test('clearsTimeoutWhenReinitialized', async function() {
      const {assertEquals} = await import('chrome://webui-test/chai_assert.js');

      const firstTermsPage = termsPage;
      firstTermsPage.loadTimeoutMs_ = 40;
      sendHostMessage({action: 'showPage', page: 'terms'});
      assertEquals(LoadState.LOADING, firstTermsPage.state_);

      window.loadTimeData.data_ = null;
      sendHostMessage({
        action: 'initialize',
        deviceId: 'test-device-2',
        data: {
          locale: 'en',
          arcManaged: false,
          countryCode: 'us',
          isOwnerProfile: true,
          learnMoreStatistics: '',
          textMetricsDisabled: '',
          textMetricsEnabled: '',
          textMetricsManagedDisabled: '',
          textMetricsManagedEnabled: '',
          learnMoreBackupAndRestore: '',
          learnMoreLocationServices: '',
          learnMorePaiService: '',
          controlledByPolicy: '',
          overlayClose: 'Close',
          serverError: 'Server error',
        },
      });
      assertEquals(null, firstTermsPage.loadTimeoutId_);

      await sleep(90);
      assertEquals(0, termsView.stopCount);
    });
  });

  mocha.run();
});
