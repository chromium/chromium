// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createApp} from './common.js';

suite('ReadabilityAxTreeAnchorsIntegration', () => {
  function setTree(rootChildren: number[], nodes: Object[]) {
    const tree = {
      rootId: 1,
      nodes: [
        {
          id: 1,
          role: 'rootWebArea',
          htmlTag: '#document',
          childIds: rootChildren,
        },
        ...nodes,
      ],
    };
    const contentNodeIds = nodes.map((n: any) => n.id);
    contentNodeIds.push(1);
    chrome.readingMode.setAnchorsForTesting(tree, contentNodeIds);
  }

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    chrome.readingMode.onConnected = () => {};
    await createApp();
  });

  test('validates AX Tree Conversion Logic', () => {
    const googleUrl = 'https://www.google.com';
    const wikiUrl = 'https://www.wikipedia.org';
    const idLinkWithContext = 12;
    const idLinkNoContext = 21;
    const idWiki = 31;

    const nodes = [
      {id: 10, role: 'paragraph', htmlTag: 'p', childIds: [11, 12, 13]},
      {id: 11, role: 'staticText', name: 'Visit '},
      {id: 12, role: 'link', htmlTag: 'a', url: googleUrl, childIds: [120]},
      {id: 120, role: 'staticText', name: 'Google'},
      {id: 13, role: 'staticText', name: ' now.'},

      {id: 20, role: 'paragraph', htmlTag: 'p', childIds: [21]},
      {id: 21, role: 'link', htmlTag: 'a', url: googleUrl, childIds: [210]},
      {id: 210, role: 'staticText', name: 'Footer'},

      {id: 30, role: 'paragraph', htmlTag: 'p', childIds: [31, 32]},
      {id: 31, role: 'link', htmlTag: 'a', url: wikiUrl, childIds: [310]},
      {id: 310, role: 'staticText', name: 'Wiki'},
      {id: 32, role: 'staticText', name: ' is free.'},
    ];

    setTree([10, 20, 30], nodes);
    const anchors: Record<string, AxTreeAnchorMetadata[]> =
        chrome.readingMode.axTreeAnchors;
    assertTrue(!!anchors, 'Anchors map should exist');

    const googleLinks = anchors[googleUrl];
    assertTrue(!!googleLinks);
    assertEquals(2, googleLinks.length);
    googleLinks.sort((a, b) => a.axId - b.axId);

    const link1 = googleLinks[0]!;
    assertEquals(idLinkWithContext, link1.axId);
    assertEquals('Visit ', link1.textBefore);
    assertEquals(' now.', link1.textAfter);

    const link2 = googleLinks[1]!;
    assertEquals(idLinkNoContext, link2.axId);
    assertFalse(!!link2.textBefore);
    assertFalse(!!link2.textAfter);

    const wikiLinks = anchors[wikiUrl];
    assertTrue(!!wikiLinks);
    assertEquals(1, wikiLinks.length);

    const wikiLink = wikiLinks[0]!;
    assertEquals(idWiki, wikiLink.axId);
    assertFalse(!!wikiLink.textBefore);
    assertEquals(' is free.', wikiLink.textAfter);
  });
});
