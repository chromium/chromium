// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ProxyView} from 'chrome://net-internals/proxy_view.js';
import {$} from 'chrome://resources/js/util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {switchToView} from './test_util.js';

suite('NetInternalsProxyViewTest', function() {
  setup(function() {
    chrome.send('setNetworkContextForTesting');
  });

  teardown(function() {
    ProxyView.getInstance().show(false);
    chrome.send('resetNetworkContextForTesting');
  });

  function isVisible(elementId) {
    return $(elementId).style.display !== 'none';
  }

  async function setProxySettingsAndUpdate(proxySettings) {
    chrome.send('setProxySettingsForTesting', [JSON.stringify(proxySettings)]);
    switchToView('proxy');
    await ProxyView.getInstance().updateProxySettings();
  }

  test('DirectSettings', async function() {
    await setProxySettingsAndUpdate({
      original: {},
      effective: {},
    });

    assertTrue(isVisible(ProxyView.EFFECTIVE_CONTENT_DIV_ID));
    assertEquals(
        'Use DIRECT connections.',
        $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
    assertFalse(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));
  });

  test('ManualSingleProxyAndBypass', async function() {
    await setProxySettingsAndUpdate({
      original: {
        single_proxy: ['http://proxy.example.com:8080'],
        bypass_list: ['*.local', '127.0.0.1'],
      },
      effective: {
        single_proxy: ['http://proxy.example.com:8080'],
        bypass_list: ['*.local', '127.0.0.1'],
      },
    });

    assertEquals(
        'Proxy server: http://proxy.example.com:8080\n' +
            'Bypass list: \n' +
            '  *.local\n' +
            '  127.0.0.1',
        $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
    assertFalse(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));

    await setProxySettingsAndUpdate({
      original: {
        single_proxy: ['http://proxy1.example.com:8080', 'direct://'],
        bypass_list: ['*.example.com'],
        reverse_bypass: true,
      },
      effective: {
        single_proxy: ['http://proxy1.example.com:8080', 'direct://'],
        bypass_list: ['*.example.com'],
        reverse_bypass: true,
      },
    });

    assertEquals(
        'Proxy server: [http://proxy1.example.com:8080, direct://]\n' +
            'Reversed bypass list: \n' +
            '  *.example.com',
        $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
  });

  test('ProxyPerScheme', async function() {
    await setProxySettingsAndUpdate({
      original: {
        proxy_per_scheme: {
          http: ['http://http-proxy:8080'],
          https: ['https://secure-proxy:8443'],
          fallback: ['socks5://fallback-proxy:1080'],
        },
      },
      effective: {
        proxy_per_scheme: {
          http: ['http://http-proxy:8080'],
          https: ['https://secure-proxy:8443'],
          fallback: ['socks5://fallback-proxy:1080'],
        },
      },
    });

    assertEquals(
        'Proxy server for HTTP: http://http-proxy:8080\n' +
            'Proxy server for HTTPS: https://secure-proxy:8443\n' +
            'Proxy server for everything else: socks5://fallback-proxy:1080',
        $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
    assertFalse(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));
  });

  test(
      'AutoDetectAndPacUrlWithOriginalDifferentFromEffective',
      async function() {
        // Simulate PAC initialization in progress (`original` present,
        // `effective` not yet resolved).
        await setProxySettingsAndUpdate({
          original: {
            auto_detect: true,
            pac_url: 'http://wpad.example.com/wpad.dat',
          },
        });

        assertTrue(isVisible(ProxyView.EFFECTIVE_CONTENT_DIV_ID));
        assertTrue(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));
        assertEquals(
            'Initializing...',
            $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
        assertEquals(
            '(1) Auto-detect\n' +
                '(2) PAC script: http://wpad.example.com/wpad.dat',
            $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).textContent);

        // Simulate PAC initialization completing with `effective` resolved.
        await setProxySettingsAndUpdate({
          original: {
            auto_detect: true,
            pac_url: 'http://wpad.example.com/wpad.dat',
          },
          effective: {
            pac_url: 'http://wpad.example.com/wpad.dat',
          },
        });

        assertTrue(isVisible(ProxyView.EFFECTIVE_CONTENT_DIV_ID));
        assertTrue(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));
        assertEquals(
            'PAC script: http://wpad.example.com/wpad.dat',
            $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
        assertEquals(
            '(1) Auto-detect\n' +
                '(2) PAC script: http://wpad.example.com/wpad.dat',
            $(ProxyView.ORIGINAL_SETTINGS_DIV_ID).textContent);

        let copiedText = '';
        const originalWriteText = navigator.clipboard.writeText;
        navigator.clipboard.writeText = text => {
          copiedText = text;
          return Promise.resolve();
        };
        try {
          $(ProxyView.COPY_EFFECTIVE_SETTINGS_BUTTON_ID).click();
          assertEquals(
              'PAC script: http://wpad.example.com/wpad.dat', copiedText);

          $(ProxyView.COPY_ORIGINAL_SETTINGS_BUTTON_ID).click();
          assertEquals(
              '(1) Auto-detect\n' +
                  '(2) PAC script: http://wpad.example.com/wpad.dat',
              copiedText);
        } finally {
          navigator.clipboard.writeText = originalWriteText;
        }
      });

  test('OverrideRulesAndDynamicRouting', async function() {
    const overrideRules = [
      {
        destination_matchers: ['https://*.example.com'],
        proxy_list: ['HTTPS proxy.example.com:443'],
      },
    ];
    const dynamicRouting = {
      routing_rules: [
        {
          matchers: ['https://app.example.com'],
          proxy: 'HTTPS proxy2.example.com:443',
        },
      ],
    };

    await setProxySettingsAndUpdate({
      original: {
        override_rules: overrideRules,
        dynamic_routing_config: dynamicRouting,
        auto_detect: true,
      },
      effective: {
        override_rules: overrideRules,
        dynamic_routing_config: dynamicRouting,
        auto_detect: true,
      },
    });

    const expectedOverrideLines =
        JSON.stringify(overrideRules, null, 2).split('\n').join('\n    ');
    const expectedDynamicLines =
        JSON.stringify(dynamicRouting, null, 2).split('\n').join('\n    ');
    const expected = `(1) Override rules:\n    ${expectedOverrideLines}\n` +
        `(2) Dynamic routing:\n    ${expectedDynamicLines}\n` +
        `(3) Auto-detect`;

    assertEquals(expected, $(ProxyView.EFFECTIVE_SETTINGS_DIV_ID).textContent);
    assertFalse(isVisible(ProxyView.ORIGINAL_CONTENT_DIV_ID));
  });

  test('BadProxies', async function() {
    await setProxySettingsAndUpdate({
      original: {
        single_proxy: ['http://proxy1.example.com:8080', 'direct://'],
      },
      effective: {
        single_proxy: ['http://proxy1.example.com:8080', 'direct://'],
      },
      badProxies: [
        {
          proxy_chain_uri: '[http://proxy1.example.com:8080]',
          bad_until: '100000',
        },
      ],
      timeTickOffset: '1700000000000',
    });

    assertTrue(isVisible(ProxyView.BAD_PROXIES_DIV_ID));
    const rows = $(ProxyView.BAD_PROXIES_TBODY_ID).querySelectorAll('tr');
    assertEquals(1, rows.length);
    const cells = rows[0].querySelectorAll('td');
    assertEquals(2, cells.length);
    assertEquals('[http://proxy1.example.com:8080]', cells[0].textContent);

    // Clicking "Clear bad proxies" should clear the badProxies list and hide
    // the bad proxies section.
    $(ProxyView.CLEAR_BAD_PROXIES_BUTTON_ID).click();
    await ProxyView.getInstance().updateProxySettings();
    assertFalse(isVisible(ProxyView.BAD_PROXIES_DIV_ID));
    assertEquals(
        0, $(ProxyView.BAD_PROXIES_TBODY_ID).querySelectorAll('tr').length);
  });
});
