// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {AxReadAloudNode, currentReadHighlightClass, MovementGranularity, NodeStore, PhraseHighlight, previousReadHighlightClass, SentenceHighlight, WordHighlight} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertGT, assertLT, assertStringContains, assertStringExcludes, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {FakeReadingMode} from './fake_reading_mode.js';

suite('Movement', () => {
  let nodeStore: NodeStore;

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    nodeStore = NodeStore.getInstance();
    nodeStore.clear();
  });

  function assertHtml(html: string, id: number) {
    assertEquals(
        html, (nodeStore.getDomNode(id) as Element).innerHTML,
        (nodeStore.getDomNode(id) as Element).innerHTML);
  }

  function assertHtmlContains(partialHtml: string, id: number) {
    assertStringContains(
        (nodeStore.getDomNode(id) as Element).innerHTML, partialHtml);
  }

  function assertHtmlExcludes(partialHtml: string, id: number) {
    assertStringExcludes(
        (nodeStore.getDomNode(id) as Element).innerHTML, partialHtml);
  }

  suite('MovementGranularity', () => {
    test('isEmpty is true when no highlights are added', () => {
      const granularity = new MovementGranularity();
      assertTrue(granularity.isEmpty());
    });

    test('isEmpty is false after adding a highlight', () => {
      const id = 1;
      const p = document.createElement('p');
      p.innerText = 'I\'m gonna make a change';
      nodeStore.setDomNode(p, id);
      const segments = [{node: new AxReadAloudNode(id), start: 0, length: 5}];

      const granularity = new MovementGranularity();
      granularity.addHighlight(new SentenceHighlight(segments));

      assertFalse(granularity.isEmpty());
    });

    test('setPrevious calls setPrevious on all highlights', () => {
      const id1 = 1;
      const p1 = document.createElement('p');
      p1.innerText = 'For once in my life.';
      nodeStore.setDomNode(p1, id1);
      const segments1 = [{
        node: new AxReadAloudNode(id1),
        start: 0,
        length: p1.innerText.length,
      }];
      const id2 = 2;
      const p2 = document.createElement('p');
      p2.innerText = 'It\'s gonna feel real good, ';
      nodeStore.setDomNode(p2, id2);
      const segments2 = [{
        node: new AxReadAloudNode(id2),
        start: 0,
        length: p2.innerText.length,
      }];
      const granularity = new MovementGranularity();
      granularity.addHighlight(new SentenceHighlight(segments1));
      granularity.addHighlight(new PhraseHighlight(segments2));

      granularity.setPrevious();

      assertHtml(
          `<span class="${
              previousReadHighlightClass}">For once in my life.</span>`,
          id1);
      assertHtml(
          `<span class="${
              previousReadHighlightClass}">It\'s gonna feel real good, </span>`,
          id2);
    });

    test('clearFormatting clears all highlights', () => {
      const id1 = 1;
      const p1 = document.createElement('p');
      p1.innerText = 'For once in my life.';
      nodeStore.setDomNode(p1, id1);
      const segments1 = [{
        node: new AxReadAloudNode(id1),
        start: 0,
        length: p1.innerText.length,
      }];
      const id2 = 2;
      const p2 = document.createElement('p');
      p2.innerText = 'It\'s gonna feel real good, ';
      nodeStore.setDomNode(p2, id2);
      const segments2 = [{
        node: new AxReadAloudNode(id2),
        start: 0,
        length: p2.innerText.length,
      }];
      const granularity = new MovementGranularity();
      granularity.addHighlight(new SentenceHighlight(segments1));
      granularity.addHighlight(new PhraseHighlight(segments2));

      granularity.clearFormatting();

      assertHtmlExcludes(currentReadHighlightClass, id1);
      assertHtmlExcludes(currentReadHighlightClass, id2);
      assertHtmlExcludes(previousReadHighlightClass, id1);
      assertHtmlExcludes(previousReadHighlightClass, id2);
    });

    test('scroll into view', () => {
      // Create a scrollable container to act as our "viewport".
      const container = document.createElement('div');
      container.style.height = window.innerHeight + 'px';
      container.style.overflow = 'scroll';
      document.body.appendChild(container);
      // Add enough content to cause scrolling.
      const spacer = document.createElement('div');
      spacer.style.height = window.innerHeight + 1 + 'px';
      container.appendChild(spacer);
      // This is the target element, initially not visible.
      const targetP = document.createElement('p');
      targetP.innerText = 'This text is off-screen.';
      container.appendChild(targetP);
      const TARGET_NODE_ID = 99;
      nodeStore.setDomNode(targetP, TARGET_NODE_ID);
      // Highlight the off-screen element
      const segments = [{
        node: new AxReadAloudNode(TARGET_NODE_ID),
        start: 0,
        length: targetP.innerText.length,
      }];
      const granularity = new MovementGranularity();
      const highlight = new SentenceHighlight(segments);
      granularity.addHighlight(highlight);
      const element = highlight.getElements().at(0);
      assertTrue(!!element);
      // The highlight should start offscreen
      assertEquals(0, container.scrollTop);
      assertFalse(granularity.isVisible());
      let bounds = element.getBoundingClientRect();
      assertGT(bounds.top, window.innerHeight);
      assertGT(bounds.bottom, window.innerHeight);

      granularity.scrollIntoView();

      // The highlight should now be fully onscreen
      assertGT(container.scrollTop, 0);
      assertTrue(granularity.isVisible());
      bounds = element.getBoundingClientRect();
      assertGT(bounds.top, 0);
      assertLT(bounds.bottom, container.scrollTop + window.innerHeight);
    });

    test('scroll into view with large highlight', () => {
      // Create a scrollable container to act as our "viewport".
      const container = document.createElement('div');
      container.style.height = window.innerHeight + 'px';
      container.style.overflow = 'scroll';
      document.body.appendChild(container);
      // Add enough content to cause scrolling.
      const spacer = document.createElement('div');
      spacer.style.height = (window.innerHeight * 2) + 'px';
      container.appendChild(spacer);
      // This is the target element, initially not visible.
      const targetP = document.createElement('p');
      const longText =
          'Well, now they know, let it go, let it go, can\'t hold it back ' +
          'anymore, let it go, let it go, turn away and slam the ' +
          'door- I don\'t care what they\'re going to say let the storm rage ' +
          'on the cold never bothered me anyway its funny how some distance ' +
          'makes everything seem small and the fears that once controlled me ' +
          'cant get to me at all- its time to see what I can do to test the ' +
          'limits and break through no right no wrong no rules for me- I\'m ' +
          'free let it go let it go I am one with the wind and sky let it go ' +
          'let it go you\'ll never see me cry- here I stand and here I stay- ' +
          'let the storm rage on';
      targetP.innerText = longText;
      container.appendChild(targetP);
      const TARGET_NODE_ID = 99;
      nodeStore.setDomNode(targetP, TARGET_NODE_ID);
      // Highlight the off-screen element
      const segments = [{
        node: new AxReadAloudNode(TARGET_NODE_ID),
        start: 0,
        length: targetP.innerText.length,
      }];
      const granularity = new MovementGranularity();
      const highlight = new SentenceHighlight(segments);
      granularity.addHighlight(highlight);
      const element = highlight.getElements().at(0);
      assertTrue(!!element);
      // The highlight should start offscreen
      assertEquals(0, container.scrollTop);
      assertFalse(granularity.isVisible());
      let bounds = element.getBoundingClientRect();
      window.innerHeight = bounds.height * 1.2;
      assertGT(bounds.top, window.innerHeight);
      assertGT(bounds.bottom, window.innerHeight);
      assertGT(bounds.height, window.innerHeight / 2);
      assertLT(bounds.height, window.innerHeight);

      granularity.scrollIntoView();

      // The highlight should now be fully onscreen
      assertGT(container.scrollTop, 0);
      assertTrue(granularity.isVisible());
      bounds = element.getBoundingClientRect();
      assertGT(bounds.top, 0);
      assertLT(bounds.bottom, container.scrollTop + window.innerHeight);
    });
  });

  suite('SentenceHighlight', () => {
    test('highlights a full sentence in one node', () => {
      const id = 1;
      const p = document.createElement('p');
      const text = 'Gonna make a difference.';
      p.innerText = text;
      nodeStore.setDomNode(p, id);
      const segments =
          [{node: new AxReadAloudNode(id), start: 0, length: text.length}];

      const highlight = new SentenceHighlight(segments);

      assertFalse(highlight.isEmpty());
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text}</span>`, id);
    });

    test('highlights a full sentence across nodes', () => {
      const id1 = 1;
      const id2 = 2;
      const text1 = 'Gonna make ';
      const text2 = 'it right.';
      const p = document.createElement('p');
      p.innerText = text1;
      const a = document.createElement('a');
      a.innerText = text2;
      nodeStore.setDomNode(p, id1);
      nodeStore.setDomNode(a, id2);
      const segments = [
        {
          node: new AxReadAloudNode(id1),
          start: 0,
          length: text1.length,
        },
        {
          node: new AxReadAloudNode(id2),
          start: 0,
          length: text2.length,
        },
      ];

      const highlight = new SentenceHighlight(segments);

      assertEquals(2, highlight.getElements().length);
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text1}</span>`, id1);
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text2}</span>`, id2);
    });

    test(
        'when highlighting only punctuation, still uses current highlight',
        () => {
          const id1 = 1;
          const id2 = 2;
          const p1 = document.createElement('p');
          const p2 = document.createElement('p');
          const text1 = 'I\'m starting with the man in the mirror';
          const text2 = '!!';
          p1.appendChild(document.createTextNode(text1));
          p2.appendChild(document.createTextNode(text2));
          nodeStore.setDomNode(p1, id1);
          nodeStore.setDomNode(p2, id2);
          const segments = [
            {node: new AxReadAloudNode(id1), start: 0, length: text1.length},
            {node: new AxReadAloudNode(id2), start: 0, length: text2.length},
          ];

          new SentenceHighlight(segments);


          assertHtmlContains(currentReadHighlightClass, id1);
          assertHtmlExcludes(previousReadHighlightClass, id1);
          assertHtmlContains(currentReadHighlightClass, id2);
          assertHtmlExcludes(previousReadHighlightClass, id2);
        });
  });

  suite('WordHighlight', () => {
    test('uses TTS length if it exists', () => {
      const id = 1;
      const p = document.createElement('p');
      const word = 'As';
      p.innerText = word + ' I turn up the collar on';
      nodeStore.setDomNode(p, id);
      // The segment covers the whole text, but the TTS length is just the word.
      const segments = [{
        node: new AxReadAloudNode(id),
        start: 0,
        length: p.innerText.length,
      }];
      const ttsWordLength = word.length;

      new WordHighlight(segments, ttsWordLength);

      assertHtml(
          `<span class="${currentReadHighlightClass}">${
              word}</span> I turn up the collar on`,
          id);
    });

    test('uses segment length if TTS length is not there', () => {
      const id = 1;
      const p = document.createElement('p');
      const word = 'My';
      p.innerText = word + ' Favorite winter coat';
      nodeStore.setDomNode(p, id);
      // The segment covers the whole text, but the TTS length is just the word.
      const segments =
          [{node: new AxReadAloudNode(id), start: 0, length: word.length}];

      new WordHighlight(segments, 0);

      assertHtml(
          `<span class="${currentReadHighlightClass}">${
              word}</span> Favorite winter coat`,
          id);
    });

    test('when highlighting only punctuation, uses previous highlight', () => {
      const id = 1;
      const p = document.createElement('p');
      const text1 = 'I\'m asking him to change his ';
      const word = 'ways';
      const text2 = '. ';
      p.appendChild(document.createTextNode(text1 + word));
      p.appendChild(document.createTextNode(text2));
      nodeStore.setDomNode(p, id);
      const segments = [
        {
          node: new AxReadAloudNode(id),
          start: text1.length,
          length: word.length,
        },
        {
          node: new AxReadAloudNode(id),
          start: text1.length + word.length,
          length: text2.length,
        },
      ];

      new WordHighlight(segments, 0);


      assertHtmlContains(previousReadHighlightClass, id);
      assertHtmlExcludes(currentReadHighlightClass, id);
    });
  });

  suite('Phrase', () => {
    test('highlights a phrase in one node', () => {
      const id = 1;
      const p = document.createElement('p');
      const text = 'Who am I';
      p.innerText = text;
      nodeStore.setDomNode(p, id);
      const segments =
          [{node: new AxReadAloudNode(id), start: 0, length: text.length}];

      const highlight = new PhraseHighlight(segments);

      assertFalse(highlight.isEmpty());
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text}</span>`, id);
    });

    test('highlights a phrase across nodes', () => {
      const id1 = 1;
      const id2 = 2;
      const text1 = 'to be ';
      const text2 = 'blind, ';
      const p = document.createElement('p');
      p.innerText = text1;
      const a = document.createElement('a');
      a.innerText = text2;
      nodeStore.setDomNode(p, id1);
      nodeStore.setDomNode(a, id2);
      const segments = [
        {
          node: new AxReadAloudNode(id1),
          start: 0,
          length: text1.length,
        },
        {
          node: new AxReadAloudNode(id2),
          start: 0,
          length: text2.length,
        },
      ];

      const highlight = new PhraseHighlight(segments);

      assertEquals(2, highlight.getElements().length);
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text1}</span>`, id1);
      assertHtml(
          `<span class="${currentReadHighlightClass}">${text2}</span>`, id2);
    });

    test('when highlighting only punctuation, uses previous highlight', () => {
      const id = 1;
      const p = document.createElement('p');
      const text1 = 'No message coulda been any ';
      const word = 'clearer';
      const text2 = '. ';
      p.appendChild(document.createTextNode(text1 + word));
      p.appendChild(document.createTextNode(text2));
      nodeStore.setDomNode(p, id);
      const segments = [
        {
          node: new AxReadAloudNode(id),
          start: text1.length,
          length: word.length,
        },
        {
          node: new AxReadAloudNode(id),
          start: text1.length + word.length,
          length: text2.length,
        },
      ];

      new PhraseHighlight(segments);

      assertHtmlContains(previousReadHighlightClass, id);
      assertHtmlExcludes(currentReadHighlightClass, id);
    });
  });

  test('setPrevious changes current highlight to previous', () => {
    const id = 1;
    const p = document.createElement('p');
    const text = 'My favorite winter coat';
    p.innerText = text;
    nodeStore.setDomNode(p, id);
    const segments =
        [{node: new AxReadAloudNode(id), start: 0, length: text.length}];

    const highlight = new SentenceHighlight(segments);
    assertHtmlContains(currentReadHighlightClass, id);
    assertHtmlExcludes(previousReadHighlightClass, id);

    highlight.setPrevious();

    assertHtmlExcludes(currentReadHighlightClass, id);
    assertHtmlContains(previousReadHighlightClass, id);
  });

  test('clearFormatting removes highlight classes', () => {
    const id = 1;
    const p = document.createElement('p');
    const text = 'This wind is blowin my mind';
    p.innerText = text;
    nodeStore.setDomNode(p, id);
    const segments =
        [{node: new AxReadAloudNode(id), start: 0, length: text.length}];

    const highlight = new SentenceHighlight(segments);
    assertHtmlContains(currentReadHighlightClass, id);

    highlight.clearFormatting();
    assertHtmlExcludes(currentReadHighlightClass, id);
  });

  test('getOffsets returns correct character offsets', () => {
    const id = 1;
    const p = document.createElement('p');
    const sentence1 = 'I see the kids in the street.';
    const sentence2 = ' Without enough to eat.';
    p.innerText = sentence1 + sentence2;
    nodeStore.setDomNode(p, id);
    const segments =
        [{node: new AxReadAloudNode(id), start: 0, length: sentence1.length}];

    const highlight = new SentenceHighlight(segments);
    const offsets = highlight.getOffsets();
    const nodes = Array.from(offsets.keys());
    const values = Array.from(offsets.values());

    // The highlight will create two new text nodes inside the spans.
    // One for the highlighted text, one for the suffix.
    assertEquals(2, offsets.size);
    assertEquals(sentence1, nodes[0]!.textContent);
    assertEquals(0, values[0]);
    assertEquals(sentence2, nodes[1]!.textContent);
    assertEquals(sentence1.length, values[1]);
  });
});
