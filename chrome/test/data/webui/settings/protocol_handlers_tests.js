// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for protocol_handlers. */
suite('ProtocolHandlers', function() {
  /**
   * A dummy protocol handler element created before each test.
   * @type {ProtocolHandlers}
   */
  let testElement;

  /**
   * A list of ProtocolEntry fixtures.
   * @type {!Array<!ProtocolEntry>}
   */
  const protocols = [
    {
      handlers: [{
        host: 'www.google.com',
        protocol: 'mailto',
        protocol_name: 'email',
        spec: 'http://www.google.com/%s',
        is_default: true
      }],
      protocol: 'mailto'
    },
    {
      handlers: [
        {
          host: 'www.google1.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google1.com/%s',
          is_default: true
        },
        {
          host: 'www.google2.com',
          protocol: 'webcal',
          protocol_name: 'web calendar',
          spec: 'http://www.google2.com/%s',
          is_default: false
        }
      ],
      protocol: 'webcal'
    }
  ];

  /**
   * A list of IngnoredProtocolEntry fixtures.
   * @type {!Array<!HandlerEntry}>}
   */
  const ignoredProtocols = [{
    host: 'www.google.com',
    protocol: 'web+ignored',
    protocol_name: 'web+ignored',
    spec: 'https://www.google.com/search?q=ignored+%s',
    is_default: false
  }];

  /**
   * The mock proxy object to use during test.
   * @type {TestSiteSettingsPrefsBrowserProxy}
   */
  let browserProxy = null;

  setup(function() {
    browserProxy = new TestSiteSettingsPrefsBrowserProxy();
    settings.SiteSettingsPrefsBrowserProxyImpl.instance_ = browserProxy;
  });

  teardown(function() {
    testElement.remove();
    testElement = null;
  });

  /** @return {!Promise} */
  function initPage() {
    browserProxy.reset();
    PolymerTest.clearBody();
    testElement = document.createElement('protocol-handlers');
    document.body.appendChild(testElement);
    return browserProxy.whenCalled('observeProtocolHandlers').then(function() {
      Polymer.dom.flush();
    });
  }

  test('empty list', function() {
    return initPage().then(function() {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      assertEquals(0, listFrames.length);
    });
  });

  test('non-empty list', function() {
    browserProxy.setProtocolHandlers(protocols);

    return initPage().then(function() {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      const listItems = testElement.root.querySelectorAll('.list-item');
      // There are two protocols: ["mailto", "webcal"].
      assertEquals(2, listFrames.length);
      // There are three total handlers within the two protocols.
      assertEquals(3, listItems.length);

      // Check that item hosts are rendered correctly.
      const hosts = testElement.root.querySelectorAll('.protocol-host');
      assertEquals('www.google.com', hosts[0].textContent.trim());
      assertEquals('www.google1.com', hosts[1].textContent.trim());
      assertEquals('www.google2.com', hosts[2].textContent.trim());

      // Check that item default subtexts are rendered correctly.
      const defText = testElement.root.querySelectorAll('.protocol-default');
      assertFalse(defText[0].hidden);
      assertFalse(defText[1].hidden);
      assertTrue(defText[2].hidden);
    });
  });

  test('non-empty ignored protocols', () => {
    browserProxy.setIgnoredProtocols(ignoredProtocols);

    return initPage().then(() => {
      const listFrames = testElement.root.querySelectorAll('.list-frame');
      const listItems = testElement.root.querySelectorAll('.list-item');
      // There is a single blocked protocols section
      assertEquals(1, listFrames.length);
      // There is one total handlers within the two protocols.
      assertEquals(1, listItems.length);

      // Check that item hosts are rendered correctly.
      const hosts = testElement.root.querySelectorAll('.protocol-host');
      assertEquals('www.google.com', hosts[0].textContent.trim());

      // Check that item default subtexts are rendered correctly.
      const defText = testElement.root.querySelectorAll('.protocol-protocol');
      assertFalse(defText[0].hidden);
    });
  });

  /**
   * A reusable function to test different action buttons.
   * @param {string} button id of the button to test.
   * @param {string} handler name of browserProxy handler to react.
   * @return {!Promise}
   */
  function testButtonFlow(button, browserProxyHandler) {
    return initPage().then(() => {
      // Initiating the elements
      const menuButtons =
          testElement.root.querySelectorAll('cr-icon-button.icon-more-vert');
      assertEquals(3, menuButtons.length);
      const dialog = testElement.$$('cr-action-menu');
      return Promise.all([[0, 0], [1, 0], [1, 1]].map((indices, menuIndex) => {
        const protocolIndex = indices[0];
        const handlerIndex = indices[1];
        // Test the button for the first protocol handler
        browserProxy.reset();
        assertFalse(dialog.open);
        menuButtons[menuIndex].click();
        assertTrue(dialog.open);
        testElement.$[button].click();
        assertFalse(dialog.open);
        return browserProxy.whenCalled(browserProxyHandler).then(args => {
          const protocol = args[0];
          const url = args[1];
          // BrowserProxy's handler is expected to be called with
          // arguments as [protocol, url].
          assertEquals(protocols[protocolIndex].protocol, protocol);
          assertEquals(
              protocols[protocolIndex].handlers[handlerIndex].spec, url);
        });
      }));
    });
  }

  test('remove button works', function() {
    browserProxy.setProtocolHandlers(protocols);
    return testButtonFlow('removeButton', 'removeProtocolHandler');
  });

  test('default button works', function() {
    browserProxy.setProtocolHandlers(protocols);
    return testButtonFlow('defaultButton', 'setProtocolDefault').then(() => {
      const menuButtons =
          testElement.root.querySelectorAll('cr-icon-button.icon-more-vert');
      const closeMenu = () => testElement.$$('cr-action-menu').close();
      menuButtons[0].click();
      assertTrue(testElement.$.defaultButton.hidden);
      closeMenu();
      menuButtons[1].click();
      assertTrue(testElement.$.defaultButton.hidden);
      closeMenu();
      menuButtons[2].click();
      assertFalse(testElement.$.defaultButton.hidden);
    });
  });

  test('remove button for ignored works', () => {
    browserProxy.setIgnoredProtocols(ignoredProtocols);
    return initPage()
        .then(() => {
          testElement.$$('#removeIgnoredButton').click();
          return browserProxy.whenCalled('removeProtocolHandler');
        })
        .then(args => {
          const protocol = args[0];
          const url = args[1];
          // BrowserProxy's handler is expected to be called with arguments as
          // [protocol, url].
          assertEquals(ignoredProtocols[0].protocol, protocol);
          assertEquals(ignoredProtocols[0].spec, url);
        });
  });
});
