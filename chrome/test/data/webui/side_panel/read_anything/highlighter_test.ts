// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import {BrowserProxy, NodeStore, previousReadHighlightClass, ReadAloudHighlighter, ReadAloudNode, setInstance, VoiceLanguageController, WordBoundaries} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertStringContains, assertStringExcludes, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';

import {createSpeechSynthesisVoice} from './common.js';
import {FakeReadingMode} from './fake_reading_mode.js';
import {TestColorUpdaterBrowserProxy} from './test_color_updater_browser_proxy.js';
import {TestReadAloudModelBrowserProxy} from './test_read_aloud_browser_proxy.js';

suite('Highlighter', () => {
  let highlighter: ReadAloudHighlighter;
  let nodeStore: NodeStore;
  let wordBoundaries: WordBoundaries;
  let voiceLanguageController: VoiceLanguageController;
  let readAloudModel: TestReadAloudModelBrowserProxy;

  function assertFullNodeIsHighlighted(id: number, text: string) {
    assertEquals(
        '<span class="current-read-highlight">' + text + '</span>',
        (nodeStore.getDomNode(id) as Element).innerHTML,
        (nodeStore.getDomNode(id) as Element).innerHTML);
  }

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

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxy.setInstance(new TestColorUpdaterBrowserProxy());
    const readingMode = new FakeReadingMode();
    chrome.readingMode = readingMode as unknown as typeof chrome.readingMode;
    chrome.readingMode.isPhraseHighlightingEnabled = true;

    readAloudModel = new TestReadAloudModelBrowserProxy();
    setInstance(readAloudModel);
    wordBoundaries = new WordBoundaries();
    WordBoundaries.setInstance(wordBoundaries);
    voiceLanguageController = new VoiceLanguageController();
    VoiceLanguageController.setInstance(voiceLanguageController);
    highlighter = new ReadAloudHighlighter();
    nodeStore = NodeStore.getInstance();
    nodeStore.clear();
  });

  test('sentence highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);

    const nodeId = 10;
    const sentence = document.createElement('p');
    const text1 = 'When my legs don\'t work like they used to before. ';
    const text2 = 'And I can\'t sweep you off of your feet.';
    sentence.appendChild(document.createTextNode(text1 + text2));
    nodeStore.setDomNode(sentence, nodeId);
    const segments = [{
      node: ReadAloudNode.create(sentence)!,
      start: text1.length,
      length: text2.length,
    }];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">' + text1 + '</span>' +
            '<span class="current-read-highlight">' + text2 + '</span>',
        nodeId);
  });

  test('sentence highlight across multiple nodes', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
    const sentence = document.createElement('p');
    const text1 = 'Will your mouth still remember ';
    sentence.appendChild(document.createTextNode(text1));
    const bold = document.createElement('b');
    const text2 = 'the taste of my love?';
    bold.appendChild(document.createTextNode(text2));
    const id1 = 10;
    const id2 = 12;
    nodeStore.setDomNode(sentence, id1);
    nodeStore.setDomNode(bold, id2);
    const segments = [
      {node: ReadAloudNode.create(sentence)!, start: 0, length: text1.length},
      {node: ReadAloudNode.create(bold)!, start: 0, length: text2.length},
    ];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertFullNodeIsHighlighted(id1, text1);
    assertFullNodeIsHighlighted(id2, text2);
  });

  test('with auto highlighting and rate of 2, sentence highlight used', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
    chrome.readingMode.onSpeechRateChange(2);

    const id = 10;
    const sentence = document.createElement('p');
    const text = 'Woke up today, feeling the way I always do. ';
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    const segments = [
      {node: ReadAloudNode.create(sentence)!, start: 0, length: text.length},
    ];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertFullNodeIsHighlighted(id, text);
  });

  test('word highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    wordBoundaries.updateBoundary(0);
    const id = 10;
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(
        'Will your your eyes still smile from your cheeks?'));
    nodeStore.setDomNode(sentence, id);
    const highlights =
        [{node: ReadAloudNode.create(sentence)!, start: 5, length: 5}];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">Will </span>' +
            '<span class="current-read-highlight">your </span>' +
            'your eyes still smile from your cheeks?',
        id);
  });

  test('word highlight with no boundaries uses sentence highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    const id = 10;
    const text = 'Welcome to the house. ';
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    const word = [{node: ReadAloudNode.create(sentence)!, start: 5, length: 5}];
    readAloudModel.setHighlightForCurrentSegmentIndex(word);
    const segments = [{
      node: ReadAloudNode.create(sentence)!,
      start: 0,
      length: text.length,
    }];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml('<span class="current-read-highlight">' + text + '</span>', id);
  });

  test('word highlight with eSpeak voice uses sentence highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    const selectedVoice =
        createSpeechSynthesisVoice({lang: 'en', name: 'Kristi eSpeak'});
    voiceLanguageController.setUserPreferredVoice(selectedVoice);
    const id = 10;
    const text = 'To the Haus of Holbein. ';
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    const word = [{node: ReadAloudNode.create(sentence)!, start: 5, length: 5}];
    readAloudModel.setHighlightForCurrentSegmentIndex(word);
    const segments = [{
      node: ReadAloudNode.create(sentence)!,
      start: 0,
      length: text.length,
    }];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml('<span class="current-read-highlight">' + text + '</span>', id);
  });

  test('word highlight with engine length', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    const engineLength = 4;
    const segmenterLength = 5;
    wordBoundaries.updateBoundary(0, engineLength);
    const id = 10;
    const sentence = document.createElement('p');
    sentence.appendChild(
        document.createTextNode('Hans Holbein goes around the world'));
    nodeStore.setDomNode(sentence, id);
    const highlights = [
      {
        node: ReadAloudNode.create(sentence)!,
        start: 0,
        length: segmenterLength,
      },
    ];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    // If the segmenter length was used, the space after Hans would be in the
    // current highlight.
    assertHtml(
        '<span class="current-read-highlight">Hans</span> Holbein goes around' +
            ' the world',
        id);
  });

  test(
      'onWillMoveToNextGranularity with word highlighting highlights the rest' +
          ' of the sentence',
      () => {
        chrome.readingMode.onHighlightGranularityChanged(
            chrome.readingMode.wordHighlighting);
        wordBoundaries.updateBoundary(0);
        const id = 10;
        const sentence = document.createElement('p');
        const text = 'Do you believe in life after love?';
        sentence.appendChild(document.createTextNode(text));
        nodeStore.setDomNode(sentence, id);
        const highlights =
            [{node: ReadAloudNode.create(sentence)!, start: 0, length: 3}];
        readAloudModel.setHighlightForCurrentSegmentIndex(highlights);
        chrome.readingMode.getTextContent = () => text;
        const segments = [
          {
            node: ReadAloudNode.create(sentence)!,
            start: 0,
            length: text.length,
          },
        ];
        readAloudModel.setCurrentTextSegments(segments);
        highlighter.highlightCurrentGranularity(
            segments,
            /*scrollIntoView=*/ false,
            /*shouldUpdateSentenceHighlight=*/ true);
        highlighter.onWillMoveToNextGranularity(segments);

        assertFalse(highlighter.hasCurrentGranularity());
        assertHtml(
            '<span class="previous-read-highlight">Do you believe in life ' +
                'after love?</span>',
            id);
      });

  test('word highlight across multiple nodes with engine length', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    // speechUtteranceLength should extend across multiple nodes.
    wordBoundaries.updateBoundary(0, 4);

    const bold = document.createElement('b');
    const text1 = 'I\'m';
    bold.appendChild(document.createTextNode(text1));
    const sentence = document.createElement('p');
    // A space is intentionally inserted into the beginning of this segment.
    const text2 = ' slipping into the lava.';
    sentence.appendChild(document.createTextNode(text2));
    const id1 = 10;
    const id2 = 12;
    nodeStore.setDomNode(bold, id1);
    nodeStore.setDomNode(sentence, id2);
    const highlights = [
      {node: ReadAloudNode.create(bold)!, start: 0, length: 3},
      {node: ReadAloudNode.create(sentence)!, start: 0, length: 1},
    ];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());

    // Only "I'm" is highlighted. The rest of the sentence, including the space
    // after "I'm" remains unhighlighted.
    assertHtml('<span class="current-read-highlight">I\'m</span>', id1);
    assertHtml(' slipping into the lava.', id2);
  });

  test('word highlight across multiple nodes without engine length', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    wordBoundaries.updateBoundary(0);

    const bold = document.createElement('b');
    const text1 = 'April';
    bold.appendChild(document.createTextNode(text1));
    const sentence = document.createElement('p');
    // A space is intentionally inserted into the beginning of this segment.
    const text2 = ' 18, 2025';
    sentence.appendChild(document.createTextNode(text2));
    const id1 = 10;
    const id2 = 12;
    nodeStore.setDomNode(bold, id1);
    nodeStore.setDomNode(sentence, id2);
    const highlights = [
      {node: ReadAloudNode.create(bold)!, start: 0, length: text1.length},
      {node: ReadAloudNode.create(sentence)!, start: 0, length: 3},
    ];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());

    assertHtml('<span class="current-read-highlight">April</span>', id1);
    assertHtml('<span class="current-read-highlight"> 18</span>, 2025', id2);
  });

  test('word highlight on punctuation only applies previous highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    wordBoundaries.updateBoundary(0);
    const id = 10;
    const sentenceText = 'And I can\'t sweep you off of your feet';
    const punctuation = '.';

    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(sentenceText + punctuation));
    nodeStore.setDomNode(sentence, id);

    // Mock the backend to return a segment for just the punctuation.
    const highlights = [{
      node: ReadAloudNode.create(sentence)!,
      start: sentenceText.length,
      length: 1,
    }];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);
    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    // There should be no "current" highlight on the page, but there is a
    // current granularity in terms of movement.
    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">' + sentenceText +
            '</span><span class="previous-read-highlight">' + punctuation +
            '</span>',
        id);
  });

  test('word highlight with single alphabet character has highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.wordHighlighting);
    wordBoundaries.updateBoundary(0);
    const id = 10;
    const sentenceText = 'I know you will';
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(sentenceText));
    nodeStore.setDomNode(sentence, id);

    const highlights = [{
      node: ReadAloudNode.create(sentence)!,
      start: 0,
      length: 1,
    }];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);
    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="current-read-highlight">I</span> know you will', id);
  });

  test('phrase highlight', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
    wordBoundaries.updateBoundary(0);
    const id = 10;
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(
        'But darling I will be loving you till we\'re 70.'));
    nodeStore.setDomNode(sentence, id);
    const highlights =
        [{node: ReadAloudNode.create(sentence)!, start: 12, length: 20}];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">But darling </span>' +
            '<span class="current-read-highlight">I will be loving you' +
            '</span> till we\'re 70.',
        id);
  });

  test('phrase highlight with engine length, ignores engine length', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
    wordBoundaries.updateBoundary(0, 1);
    const id = 10;
    const sentence = document.createElement('p');
    sentence.appendChild(document.createTextNode(
        'But darling I will be loving you till we\'re 70.'));
    nodeStore.setDomNode(sentence, id);
    const highlights =
        [{node: ReadAloudNode.create(sentence)!, start: 12, length: 20}];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">But darling </span>' +
            '<span class="current-read-highlight">I will be loving you' +
            '</span> till we\'re 70.',
        id);
  });

  test(
      'onWillMoveToNextGranularity with phrase highlighting highlights the ' +
          'rest of the sentence',
      () => {
        chrome.readingMode.onHighlightGranularityChanged(
            chrome.readingMode.autoHighlighting);
        wordBoundaries.updateBoundary(0);
        const id = 10;
        const sentence = document.createElement('p');
        const text = 'I can feel something inside me say';
        sentence.appendChild(document.createTextNode(text));
        nodeStore.setDomNode(sentence, id);
        const highlights =
            [{node: ReadAloudNode.create(sentence)!, start: 0, length: 2}];
        readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

        chrome.readingMode.getTextContent = () => text;
        const segments = [
          {
            node: ReadAloudNode.create(sentence)!,
            start: 0,
            length: text.length,
          },
        ];
        readAloudModel.setCurrentTextSegments(segments);
        highlighter.highlightCurrentGranularity(
            segments,
            /*scrollIntoView=*/ false,
            /*shouldUpdateSentenceHighlight=*/ true);
        highlighter.onWillMoveToNextGranularity(segments);

        assertFalse(highlighter.hasCurrentGranularity());
        assertHtml(
            '<span class="previous-read-highlight">I can feel something ' +
                'inside me say</span>',
            id);
      });

  test('phrase highlight across multiple nodes', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.autoHighlighting);
    // speechUtteranceLength should extend across multiple nodes.
    wordBoundaries.updateBoundary(0, 4);
    const id1 = 10;
    const id2 = 12;
    const sentence = document.createElement('p');
    const text1 = 'And honey your soul';
    sentence.appendChild(document.createTextNode(text1));
    const bold = document.createElement('b');
    const text2 = ' could never grow old, it\'s evergreen.';
    bold.appendChild(document.createTextNode(text2));
    nodeStore.setDomNode(sentence, id1);
    nodeStore.setDomNode(bold, id2);
    const highlights = [
      {node: ReadAloudNode.create(sentence)!, start: 10, length: 9},
      {node: ReadAloudNode.create(bold)!, start: 0, length: 23},
    ];
    readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

    highlighter.highlightCurrentGranularity(
        highlights,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);


    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">And honey </span>' +
            '<span class="current-read-highlight">your soul</span>',
        id1);
    assertHtml(
        '<span class="current-read-highlight"> could never grow old, </span>' +
            'it\'s evergreen.',
        id2);
  });

  test(
      'with auto highlighting and rate of 1, word/phrase highlight used',
      () => {
        chrome.readingMode.onHighlightGranularityChanged(
            chrome.readingMode.autoHighlighting);
        chrome.readingMode.onSpeechRateChange(1);
        wordBoundaries.updateBoundary(0);
        const id = 10;
        const sentence = document.createElement('p');
        sentence.appendChild(document.createTextNode(
            'Hungry for something that I can\'t eat. '));
        nodeStore.setDomNode(sentence, id);
        const highlights =
            [{node: ReadAloudNode.create(sentence)!, start: 0, length: 20}];
        readAloudModel.setHighlightForCurrentSegmentIndex(highlights);

        highlighter.highlightCurrentGranularity(
            [],
            /*scrollIntoView=*/ false,
            /*shouldUpdateSentenceHighlight=*/ true);

        assertTrue(highlighter.hasCurrentGranularity());
        assertHtml(
            '<span class="current-read-highlight">Hungry for something</span>' +
                ' that I can\'t eat. ',
            id);
      });

  test('with highlight off, sentence highlight used', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.noHighlighting);

    const nodeId = 10;
    const sentence = document.createElement('p');
    const text1 = 'I\'ve been just one word. ';
    const text2 = 'In a stupid rhyme. ';
    sentence.appendChild(document.createTextNode(text1 + text2));
    nodeStore.setDomNode(sentence, nodeId);
    const segments = [{
      node: ReadAloudNode.create(sentence)!,
      start: text1.length,
      length: text2.length,
    }];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);

    assertTrue(highlighter.hasCurrentGranularity());
    assertHtml(
        '<span class="previous-read-highlight">' + text1 + '</span>' +
            '<span class="current-read-highlight">' + text2 + '</span>',
        nodeId);
  });

  test('clear highlights', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
    const id = 10;
    const sentence = document.createElement('p');
    const text1 =
        'I\'m thinkin bout how people fall in love in mysterious ways.';
    const text2 = ' Maybe just a touch of the hand';
    sentence.appendChild(document.createTextNode(text1 + text2));
    nodeStore.setDomNode(sentence, id);
    const segments = [{
      node: ReadAloudNode.create(sentence)!,
      start: text1.length,
      length: text1.length + text2.length,
    }];
    readAloudModel.setCurrentTextSegments(segments);

    highlighter.highlightCurrentGranularity(
        segments,
        /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);
    highlighter.clearHighlightFormatting();

    assertFalse(highlighter.hasCurrentGranularity());
    assertHtmlExcludes(previousReadHighlightClass, id);
  });

  test('reset clears all highlights and state', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
    const id = 10;
    const sentence = document.createElement('p');
    const text = 'This is a sentence.';
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    const segments = [
      {node: ReadAloudNode.create(sentence)!, start: 0, length: text.length},
    ];
    readAloudModel.setCurrentTextSegments(segments);
    highlighter.highlightCurrentGranularity(
        segments, /*scrollIntoView=*/ false,
        /*shouldUpdateSentenceHighlight=*/ true);
    assertTrue(highlighter.hasCurrentGranularity());

    highlighter.reset();

    assertFalse(highlighter.hasCurrentGranularity());
    assertHtmlExcludes(previousReadHighlightClass, id);
  });

  test('restorePreviousHighlighting reapplies formatting after clear', () => {
    chrome.readingMode.onHighlightGranularityChanged(
        chrome.readingMode.sentenceHighlighting);
    const id = 10;
    const sentence = document.createElement('p');
    const text = 'This is a sentence.';
    sentence.appendChild(document.createTextNode(text));
    nodeStore.setDomNode(sentence, id);
    const segments = [
      {node: ReadAloudNode.create(sentence)!, start: 0, length: text.length},
    ];
    readAloudModel.setCurrentTextSegments(segments);
    highlighter.highlightCurrentGranularity(segments, false, true);
    highlighter.onWillMoveToNextGranularity(segments);

    // This should clear formatting but keep the granularity in the queue.
    highlighter.clearHighlightFormatting();
    highlighter.restorePreviousHighlighting();

    assertHtmlContains(previousReadHighlightClass, id);
  });

  test(
      'restorePreviousHighlighting does not reapply formatting after reset',
      () => {
        chrome.readingMode.onHighlightGranularityChanged(
            chrome.readingMode.sentenceHighlighting);
        const id = 10;
        const sentence = document.createElement('p');
        const text = 'This is a sentence.';
        sentence.appendChild(document.createTextNode(text));
        nodeStore.setDomNode(sentence, id);
        const segments = [
          {
            node: ReadAloudNode.create(sentence)!,
            start: 0,
            length: text.length,
          },
        ];
        readAloudModel.setCurrentTextSegments(segments);
        highlighter.highlightCurrentGranularity(segments, false, true);
        highlighter.onWillMoveToNextGranularity(segments);

        highlighter.reset();
        highlighter.restorePreviousHighlighting();

        assertHtmlExcludes(previousReadHighlightClass, id);
      });

  test(
      'onWillMoveToPreviousGranularity clears current and previous highlight',
      () => {
        chrome.readingMode.onHighlightGranularityChanged(
            chrome.readingMode.sentenceHighlighting);
        const id = 10;
        const sentence = document.createElement('p');
        const text1 = 'Me I fall in love with you every single day.';
        const text2 = ' And I just wanna tell you I am';
        sentence.appendChild(document.createTextNode(text1 + text2));
        nodeStore.setDomNode(sentence, id);
        const sentence1 = [{
          node: ReadAloudNode.create(sentence)!,
          start: 0,
          length: text1.length,
        }];
        const sentence2 = [{
          node: ReadAloudNode.create(sentence)!,
          start: text1.length + 1,
          length: text2.length,
        }];
        readAloudModel.setCurrentTextSegments(sentence1);

        highlighter.highlightCurrentGranularity(
            sentence1,
            /*scrollIntoView=*/ false,
            /*shouldUpdateSentenceHighlight=*/ true);
        highlighter.onWillMoveToNextGranularity(sentence1);
        highlighter.highlightCurrentGranularity(
            sentence2,
            /*scrollIntoView=*/ false,
            /*shouldUpdateSentenceHighlight=*/ true);
        highlighter.onWillMoveToPreviousGranularity();

        assertFalse(highlighter.hasCurrentGranularity());
        assertHtmlExcludes(previousReadHighlightClass, id);
      });
});
