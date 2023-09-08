// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {Paths, PersonalizationRouterElement, UserSubpageElement} from 'chrome://personalization/js/personalization_app.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('UserSubpageElementTest', function() {
  let userSubpageElement: UserSubpageElement|null;

  let personalizationStore: TestPersonalizationStore;

  let reloadAtRootPromise: Promise<void>;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;

    reloadAtRootPromise = new Promise((resolve) => {
      PersonalizationRouterElement.reloadAtRoot = resolve;
    });
  });

  teardown(async () => {
    await teardownElement(userSubpageElement);
    userSubpageElement = null;
  });

  test('displays content when not enterprise managed', async () => {
    personalizationStore.data.user.imageIsEnterpriseManaged = false;
    userSubpageElement = initElement(UserSubpageElement, {path: Paths.USER});
    await waitAfterNextRender(userSubpageElement);
    const userPreview =
        userSubpageElement.shadowRoot!.querySelector('user-preview');
    assertTrue(!!userPreview);
    const avatarList =
        userSubpageElement.shadowRoot!.querySelector('avatar-list');
    assertTrue(!!avatarList);
  });

  test('does not display content when enterprise managed', async () => {
    // Enterprise managed state is unknown.
    personalizationStore.data.user.imageIsEnterpriseManaged = null;
    userSubpageElement = initElement(UserSubpageElement, {path: Paths.USER});
    await waitAfterNextRender(userSubpageElement);

    // No user preview element.
    assertFalse(!!userSubpageElement.shadowRoot!.querySelector('user-preview'));

    personalizationStore.data.user.imageIsEnterpriseManaged = true;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(userSubpageElement);

    // Still nouser preview element.
    assertFalse(!!userSubpageElement.shadowRoot!.querySelector('user-preview'));
  });

  test('redirects to root if enterprise managed', async () => {
    personalizationStore.data.user.imageIsEnterpriseManaged = true;
    userSubpageElement = initElement(UserSubpageElement, {path: Paths.USER});
    await reloadAtRootPromise;
  });
});
