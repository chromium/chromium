// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://glic/inline_fulfillment_host.js';

import {GenericAssociatedInterface, GenericAssociatedInterfaceRemote} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {GenericPendingAssociatedReceiver} from '//resources/mojo/mojo/public/mojom/base/generic_pending_associated_receiver.mojom-webui.js';
import type {InlineFulfillmentHostElement, InlineFulfillmentModule, InlineFulfillmentRemote} from 'chrome-untrusted://glic/inline_fulfillment_host.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

class FakeRemote implements InlineFulfillmentRemote {
  closed = false;
  private remote_ = new GenericAssociatedInterfaceRemote();
  $ = {
    associateAndPassReceiver: () => this.remote_.$.associateAndPassReceiver(),
    close:
        () => {
          this.closed = true;
          this.remote_.$.close();
        },
  };
}

suite('InlineFulfillmentHost', () => {
  let host: InlineFulfillmentHostElement;
  let sent: GenericPendingAssociatedReceiver[];
  // The remotes of the cards created, in order.
  let remotes: FakeRemote[];

  function send(channel: GenericPendingAssociatedReceiver) {
    sent.push(channel);
  }

  function fakeModule(id: string): InlineFulfillmentModule {
    return {
      create() {
        const element = document.createElement('div');
        element.id = id;
        const remote = new FakeRemote();
        remotes.push(remote);
        return {
          element,
          interfaceName: GenericAssociatedInterface.$interfaceName,
          remote,
        };
      },
    };
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sent = [];
    remotes = [];
    host = document.createElement('glic-inline-fulfillment-host');
    host.loaders = new Map([
      ['a.js', () => Promise.resolve(fakeModule('a'))],
      ['b.js', () => Promise.resolve(fakeModule('b'))],
      ['broken.js', () => Promise.reject(new Error('load failed'))],
      [
        'throws.js',
        () => Promise.resolve({
          create() {
            throw new Error('create failed');
          },
        }),
      ],
    ]);
    document.body.appendChild(host);
  });

  test('ShowsCardAndSendsChannel', async () => {
    await host.show('a.js', send);
    assertEquals(1, sent.length);
    assertEquals(
        GenericAssociatedInterface.$interfaceName, sent[0]!.interfaceName);
    assertEquals(1, host.children.length);
    assertEquals('a', host.children[0]!.id);
  });

  test('SendsChannelBeforeShowingCard', async () => {
    let cardsWhenSent = -1;
    await host.show('a.js', () => cardsWhenSent = host.children.length);
    assertEquals(0, cardsWhenSent);
    assertEquals(1, host.children.length);
  });

  test('UnknownResourceShowsNothing', async () => {
    await host.show('unknown.js', send);
    assertEquals(0, sent.length);
    assertEquals(0, host.children.length);
  });

  test('FailedLoadShowsNothing', async () => {
    await host.show('broken.js', send);
    assertEquals(0, sent.length);
    assertEquals(0, host.children.length);
  });

  test('ThrowingCreateShowsNothing', async () => {
    await host.show('throws.js', send);
    assertEquals(0, sent.length);
    assertEquals(0, host.children.length);
  });

  test('ThrowingSendShowsNothingAndClosesRemote', async () => {
    await host.show('a.js', () => {
      throw new Error('send failed');
    });
    assertEquals(0, host.children.length);
    assertEquals(1, remotes.length);
    assertTrue(remotes[0]!.closed);
  });

  test('ClearRemovesCardAndClosesRemote', async () => {
    await host.show('a.js', send);
    host.clear();
    assertEquals(0, host.children.length);
    assertTrue(remotes[0]!.closed);
  });

  test('NewCardClosesOldRemote', async () => {
    await host.show('a.js', send);
    await host.show('b.js', send);
    assertEquals(1, host.children.length);
    assertEquals('b', host.children[0]!.id);
    assertTrue(remotes[0]!.closed);
    assertFalse(remotes[1]!.closed);
  });

  test('ClearDropsLoadInFlight', async () => {
    const pending = host.show('a.js', send);
    host.clear();
    await pending;
    assertEquals(0, sent.length);
    assertEquals(0, host.children.length);
    assertEquals(0, remotes.length);
  });

  test('SecondShowWins', async () => {
    await Promise.all([host.show('a.js', send), host.show('b.js', send)]);
    assertEquals(1, sent.length);
    assertEquals(1, host.children.length);
    assertEquals('b', host.children[0]!.id);
    assertEquals(1, remotes.length);
  });
});
