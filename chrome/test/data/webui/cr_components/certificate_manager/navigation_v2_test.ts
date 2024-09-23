// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
import 'chrome://resources/cr_components/certificate_manager/navigation_v2.js';

import {Page, Route, RouteObserverMixin, Router} from 'chrome://resources/cr_components/certificate_manager/navigation_v2.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const TestElementBase = RouteObserverMixin(PolymerElement);
class TestElement extends TestElementBase {
  static get properties() {
    return {
      newRoute: Object,
      oldRoute: Object,
    };
  }

  newRoute: Route|undefined;
  oldRoute: Route|undefined;

  override currentRouteChanged(newRoute: Route, oldRoute: Route): void {
    this.newRoute = newRoute;
    this.oldRoute = oldRoute;
  }
}
customElements.define('test-element', TestElement);

suite('NavigationV2Test', () => {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function checkPathAndParsing(page: Page) {
    assertEquals(
        page, Router.getPageFromPath((new Route(page)).path()),
        'path parsing for page ' + page + ' is not reversible');
  }

  test('Path parsing', () => {
    checkPathAndParsing(Page.LOCAL_CERTS);
    checkPathAndParsing(Page.CLIENT_CERTS);
    checkPathAndParsing(Page.CRS_CERTS);
    checkPathAndParsing(Page.ADMIN_CERTS);
    checkPathAndParsing(Page.PLATFORM_CERTS);
    checkPathAndParsing(Page.PLATFORM_CLIENT_CERTS);
  });

  test('navigating notifies observers', function() {
    const testElement = document.createElement('test-element') as TestElement;
    document.body.appendChild(testElement);
    flushTasks();

    Router.getInstance().navigateTo(Page.CRS_CERTS);
    assertEquals(Page.CRS_CERTS, Router.getInstance().currentRoute.page);
    assertTrue(!!testElement.newRoute);
    assertEquals(Page.CRS_CERTS, testElement.newRoute.page);
    assertTrue(!!testElement.oldRoute);
    assertEquals(Page.LOCAL_CERTS, testElement.oldRoute.page);
  });

  test('Invalid path ignored', function() {
    window.history.replaceState({}, '', 'invalid-page');

    // Create a new router to simulate opening on the invalid page.
    const router = new Router();
    assertEquals(Page.LOCAL_CERTS, router.currentRoute.page);
  });

  test('Direct navigation to client certs page', function() {
    window.history.replaceState({}, '', '/clientcerts');

    // Create a new router to simulate opening a new page.
    const router = new Router();
    assertEquals(Page.CLIENT_CERTS, router.currentRoute.page);
  });

  test('Direct navigation to admin certs subpage', function() {
    window.history.replaceState({}, '', '/localcerts/admincerts');

    // Create a new router to simulate opening a new page.
    const router = new Router();
    assertEquals(Page.ADMIN_CERTS, router.currentRoute.page);
  });

  test('test popstate changes', function(done) {
    // Create a new router to simulate opening a new page.
    const router = new Router();
    router.navigateTo(Page.CRS_CERTS);
    assertEquals(Page.CRS_CERTS, router.currentRoute.page);
    router.navigateTo(Page.CLIENT_CERTS);
    assertEquals(Page.CLIENT_CERTS, router.currentRoute.page);

    window.addEventListener('popstate', function() {
      assertEquals(Page.CRS_CERTS, router.currentRoute.page);
      assertTrue(!!router.previousRoute);
      assertEquals(Page.CLIENT_CERTS, router.previousRoute.page);
      done();
    });
    window.history.back();
  });
});
