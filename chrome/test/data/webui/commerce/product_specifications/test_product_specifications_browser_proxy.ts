// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter} from '//resources/cr_components/commerce/product_specifications.mojom-webui.js';
import type {DisclosureVersion, PageRemote, ShowSetDisposition} from '//resources/cr_components/commerce/product_specifications.mojom-webui.js';
import type {ProductSpecificationsBrowserProxy} from '//resources/cr_components/commerce/product_specifications_browser_proxy.js';
import type {Uuid} from '//resources/mojo/mojo/public/mojom/base/uuid.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestProductSpecificationsBrowserProxy extends TestBrowserProxy
    implements ProductSpecificationsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;

  constructor() {
    super([
      'showProductSpecificationsSetForUuid',
      'showProductSpecificationsSetsForUuids',
      'showComparePage',
      'setAcceptedDisclosureVersion',
      'maybeShowDisclosure',
      'declineDisclosure',
      'showSyncSetupFlow',
      'getPageTitleFromHistory',
      'getComparisonTableUrlForUuid',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }

  getCallbackRouterRemote(): PageRemote {
    return this.callbackRouterRemote;
  }

  showProductSpecificationsSetForUuid(uuid: Uuid, inNewTab: boolean): void {
    this.methodCalled('showProductSpecificationsSetForUuid', uuid, inNewTab);
  }

  showProductSpecificationsSetsForUuids(
      uuids: Uuid[], disposition: ShowSetDisposition): void {
    this.methodCalled(
        'showProductSpecificationsSetsForUuids', uuids, disposition);
  }

  showComparePage(inNewTab: boolean): void {
    this.methodCalled('showComparePage', inNewTab);
  }

  setAcceptedDisclosureVersion(version: DisclosureVersion): void {
    this.methodCalled('setAcceptedDisclosureVersion', version);
  }

  maybeShowDisclosure(urls: Url[], name: string, setId: string):
      Promise<{disclosureShown: boolean}> {
    this.methodCalled('maybeShowDisclosure', urls, name, setId);
    return Promise.resolve({disclosureShown: false});
  }

  declineDisclosure(): void {
    this.methodCalled('declineDisclosure');
  }

  showSyncSetupFlow(): void {
    this.methodCalled('showSyncSetupFlow');
  }

  getPageTitleFromHistory(url: Url): Promise<{title: string}> {
    this.methodCalled('getPageTitleFromHistory', url);
    return Promise.resolve({title: ''});
  }

  getComparisonTableUrlForUuid(uuid: Uuid): Promise<{url: Url}> {
    this.methodCalled('getComparisonTableUrlForUuid', uuid);
    return Promise.resolve({
      url: {
        url: `chrome://compare/?id=${uuid.value}`,
      },
    });
  }
}
