// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {ActorHandlerCallbackRouter} from '//webui-test/glic/glic.mojom-webui.js';
import type {ActorHandlerPendingReceiver, PerformActionsOptions as PerformActionsOptionsMojo, WebClientHandlerRemote, WebClientInitialState} from '//webui-test/glic/glic.mojom-webui.js';
import {GlicBrowserHostActor} from '//webui-test/glic/glic_api_impl/actor/actor_client.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';

suite('GlicBrowserHostActorPerformActions', () => {
  let router: ActorHandlerCallbackRouter;
  let actor: GlicBrowserHostActor;

  setup(() => {
    router = new ActorHandlerCallbackRouter();
    actor = new GlicBrowserHostActor();
    const handler = {
      createActorHandler(receiver: ActorHandlerPendingReceiver) {
        router.$.bindHandle(receiver.handle);
      },
    };
    actor.initialize(
        {enableActInFocusedTab: true} as unknown as WebClientInitialState,
        handler as unknown as WebClientHandlerRemote);
  });

  teardown(() => {
    actor.destroyActor();
    router.$.close();
  });

  // Calls performActions() and returns the options received by the browser.
  // The browser never responds, so the result of performActions() is ignored.
  function performActionsAndGetMojoOptions(...args: []|[{turnId?: string}]):
      Promise<PerformActionsOptionsMojo|null> {
    const received = new PromiseResolver<PerformActionsOptionsMojo|null>();
    router.performActions.addListener(
        (_actionsProto: number[], options: PerformActionsOptionsMojo|null) => {
          received.resolve(options);
          return new Promise(() => {});
        });
    const performActions = actor.performActions;
    assert(performActions);
    performActions.call(actor, new ArrayBuffer(0), ...args).catch(() => {});
    return received.promise;
  }

  test('forwards turnId to the browser', async () => {
    assertDeepEquals(
        {turnId: 'r_0123abcd'},
        await performActionsAndGetMojoOptions({turnId: 'r_0123abcd'}));
  });

  test('forwards an empty turnId', async () => {
    // The browser decides how to handle empty IDs.
    assertDeepEquals(
        {turnId: ''}, await performActionsAndGetMojoOptions({turnId: ''}));
  });

  test('sends an empty turnId for options without one', async () => {
    assertDeepEquals({turnId: ''}, await performActionsAndGetMojoOptions({}));
  });

  test('sends null options for the one-argument form', async () => {
    assertEquals(null, await performActionsAndGetMojoOptions());
  });
});
