// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteDataTest', function() {
  /** @type {SiteDataElement} */
  let siteData;

  /** @type {TestLocalDataBrowserProxy} */
  let testBrowserProxy;

  setup(function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS);
    testBrowserProxy = new TestLocalDataBrowserProxy();
    settings.LocalDataBrowserProxyImpl.instance_ = testBrowserProxy;
    siteData = document.createElement('site-data');
  });

  teardown(function() {
    siteData.remove();
  });

  test('remove button (trash) calls remove on origin', function() {
    const promise =
        test_util.eventToPromise('site-data-list-complete', siteData)
            .then(() => {
              Polymer.dom.flush();
              const button =
                  siteData.$$('site-data-entry').$$('.icon-delete-gray');
              assertTrue(!!button);
              assertEquals('CR-ICON-BUTTON', button.tagName);
              button.click();
              return testBrowserProxy.whenCalled('removeItem');
            })
            .then(function(path) {
              assertEquals('Hello', path);
            });
    const sites = [
      {site: 'Hello', id: '1', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    return promise;
  });

  test('remove button hidden when no search results', function() {
    const promise =
        test_util.eventToPromise('site-data-list-complete', siteData)
            .then(() => {
              assertEquals(2, siteData.$.list.items.length);
              const promise2 =
                  test_util.eventToPromise('site-data-list-complete', siteData);
              siteData.filter = 'Hello';
              return promise2;
            })
            .then(() => {
              assertEquals(1, siteData.$.list.items.length);
            });
    const sites = [
      {site: 'Hello', id: '1', localData: 'Cookiez!'},
      {site: 'World', id: '2', localData: 'Cookiez!'},
    ];
    testBrowserProxy.setCookieList(sites);
    document.body.appendChild(siteData);
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    return promise;
  });

  test('calls reloadCookies() when created', function() {
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    document.body.appendChild(siteData);
    settings.navigateTo(settings.routes.SITE_SETTINGS_COOKIES);
    return testBrowserProxy.whenCalled('reloadCookies');
  });

  test('calls reloadCookies() when visited again', function() {
    document.body.appendChild(siteData);
    settings.navigateTo(settings.routes.SITE_SETTINGS_COOKIES);
    testBrowserProxy.reset();
    settings.navigateTo(settings.routes.SITE_SETTINGS_SITE_DATA);
    return testBrowserProxy.whenCalled('reloadCookies');
  });
});
