// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {getReadAloudModel, ReadAloudNode, setInstance} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import type {DomReadAloudNode} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';


suite('ReadAloudModel', () => {
  function assertTextEmpty() {
    assertEquals('', getReadAloudModel().getCurrentTextContent());
    assertEquals(0, getReadAloudModel().getCurrentTextSegments().length);
  }

  interface TestSegment {
    node: Node;
    start: number;
    length: number;
  }

  function expectHighlightAtIndexMatches(
      index: number, expectedSegments: TestSegment[], phrases = false) {
    const segments =
        getReadAloudModel().getHighlightForCurrentSegmentIndex(index, phrases);
    assertEquals(expectedSegments.length, segments.length);
    for (let i = 0; i < segments.length; i++) {
      const expected = expectedSegments[i]!;
      const actual = segments[i]!;
      assertEquals(expected.node, actual.node.domNode());
      assertEquals(expected.start, actual.start);
      assertEquals(expected.length, actual.length);
    }
  }

  function expectHighlightAtIndexMatchesEmpty(index: number, phrases = false) {
    expectHighlightAtIndexMatches(index, [], phrases);
  }

  function assertSentenceMatchesEntireSegment(sentence: string) {
    // Remove all trailing whitespace and newline characters. It's possible
    // that extra newlines may get added to the ends of sentences to ensure
    // sentence segments are broken up correctly, but it isn't necessary
    // to test for the presence of these new lines, as testing the text in
    // a segment will verify that the text was segmented at the correct
    // sentence boundaries.
    sentence = sentence.trim();
    assertEquals(sentence, getReadAloudModel().getCurrentTextContent().trim());
    assertEquals(1, getReadAloudModel().getCurrentTextSegments().length);
    const node = getReadAloudModel().getCurrentTextSegments()[0]!.node as
        DomReadAloudNode;
    assertEquals(sentence, node.getText().trim());
  }

  // Creates a linked citation intended to mimic citations in Wikipedia and
  // other articles. Of the format:
  // <sup><a><span>[</span>text content<span>]</span></a></sup>
  function createLinkedCitationSuperscript(text: string): HTMLElement {
    const superscript = document.createElement('sup');
    const link = document.createElement('a');
    const openingBracketSpan = document.createElement('span');
    const textSpan = document.createTextNode(text);
    const closingBracketSpan = document.createElement('span');

    openingBracketSpan.textContent = '[';
    closingBracketSpan.textContent = ']';

    link.appendChild(openingBracketSpan);
    link.appendChild(textSpan);
    link.appendChild(closingBracketSpan);

    superscript.appendChild(link);
    return superscript;
  }

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setInstance(null);
  });

  test('getCurrentText opening punctuation ignored', async () => {
    const paragraph = document.createElement('p');

    const sentence = document.createTextNode('Run, take cover.');
    const parenthetical =
        document.createTextNode('(I\'m gonna come up with a plan.)');

    paragraph.appendChild(sentence);
    paragraph.appendChild(parenthetical);
    document.body.appendChild(paragraph);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    assertEquals(
        sentence.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        parenthetical.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText multiple opening punctuation ignored', async () => {
    const paragraph = document.createElement('p');

    const sentence = document.createTextNode('Run, take cover.');
    const parenthetical =
        document.createTextNode('[{<(((I\'m gonna come up with a plan.)');

    paragraph.appendChild(sentence);
    paragraph.appendChild(parenthetical);
    document.body.appendChild(paragraph);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    assertEquals(
        sentence.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        parenthetical.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test(
      'getCurrentText opening punctuation included when entire node',
      async () => {
        const paragraph = document.createElement('p');

        const sentence = document.createTextNode('And I am almost there.');
        const parenthetical = document.createElement('b');
        parenthetical.appendChild(document.createTextNode('['));
        parenthetical.appendChild(document.createTextNode('2'));
        parenthetical.appendChild(document.createTextNode(']'));

        paragraph.appendChild(sentence);
        paragraph.appendChild(parenthetical);
        document.body.appendChild(paragraph);

        await microtasksFinished();

        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        assertEquals(
            sentence.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // The next segment contains the entire bracketed statement '[2]' with
        // both opening and closing brackets so neither bracket is read
        // out-of-context.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            parenthetical.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentText with multiple opening punctuation characters',
      async () => {
        const paragraph = document.createElement('p');

        const sentence = document.createTextNode('And I am almost there.');
        const parenthetical = document.createElement('b');
        parenthetical.appendChild(document.createTextNode('['));
        parenthetical.appendChild(document.createTextNode('('));
        parenthetical.appendChild(document.createTextNode('{'));
        parenthetical.appendChild(document.createTextNode('<'));
        parenthetical.appendChild(document.createTextNode('2'));
        parenthetical.appendChild(document.createTextNode(')'));
        parenthetical.appendChild(document.createTextNode(']'));

        paragraph.appendChild(sentence);
        paragraph.appendChild(parenthetical);
        document.body.appendChild(paragraph);

        await microtasksFinished();

        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        assertEquals(
            sentence.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // The next segment contains the entire bracketed statement '[2]' with
        // both opening and closing brackets so neither bracket is read
        // out-of-context.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            parenthetical.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextContent when called many times returns same text',
      async () => {
        const header = document.createElement('h1');
        header.textContent =
            'Run, run brother- you gotta get out while you can.';

        const paragraph = document.createElement('div');
        const bold = document.createElement('strong');
        bold.textContent = 'Run, take cover. I\'m gonna come up with a plan';

        const textAfterBold = document.createTextNode('I hate to make you go.');
        paragraph.appendChild(bold);
        paragraph.appendChild(textAfterBold);

        document.body.appendChild(header);
        document.body.appendChild(paragraph);

        await microtasksFinished();

        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // Returned text is the same no matter how many times
        // getCurrentTextContent is called.
        for (let i = 0; i < 10; i++) {
          assertEquals(
              header.textContent.trim(),
              getReadAloudModel().getCurrentTextContent().trim());
        }

        // After moving speech forward, text is the same  no matter how many
        // times getCurrentTextContent is called, when the sentence does not
        // span the entire node.
        getReadAloudModel().moveSpeechForward();
        for (let i = 0; i < 10; i++) {
          assertEquals(
              'Run, take cover.',
              getReadAloudModel().getCurrentTextContent().trim());
        }
      });

  test('getCurrentTextContent returns expected text', async () => {
    const header = document.createElement('a');
    header.textContent = 'But there ain\'t no other way.';

    const paragraph = document.createElement('div');
    const bold = document.createElement('i');
    bold.textContent = 'Even though it kills me to say... ';

    const textAfterBold = document.createTextNode('Run, run brother.');
    paragraph.appendChild(bold);
    paragraph.appendChild(textAfterBold);

    document.body.appendChild(header);
    document.body.appendChild(paragraph);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        header.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // Move to the next node.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        bold.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // Move to the last node.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        textAfterBold.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test(
      'getTextContent splits sentences across line-breaking items',
      async () => {
        const header = document.createElement('h3');
        header.textContent = 'Vuelo';

        const paragraph = document.createElement('div');
        const bold = document.createElement('strong');
        bold.textContent = 'cada vez';

        const textBeforeBold = document.createTextNode('rodar entre nubez ');
        const textAfterBold = document.createTextNode(' más lejos.');
        paragraph.appendChild(textBeforeBold);
        paragraph.appendChild(bold);
        paragraph.appendChild(textAfterBold);

        document.body.appendChild(header);
        document.body.appendChild(paragraph);

        await microtasksFinished();

        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        assertEquals('Vuelo\n', getReadAloudModel().getCurrentTextContent());
      });


  test('onNodeWillBeDeleted deletes node', async () => {
    const paragraph1 = document.createElement('div');
    paragraph1.textContent = 'You need help ';

    const paragraph2 = document.createElement('div');
    const bold = document.createElement('strong');
    bold.textContent = 'I can\'t provide. ';

    const textAfterBold = document.createTextNode('I am not qualified. ');
    paragraph2.appendChild(bold);
    paragraph2.appendChild(textAfterBold);

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    assertEquals(
        'You need help \n', getReadAloudModel().getCurrentTextContent());

    // Delete the first child of paragraph1 to coincide with deleting the
    // child text node.
    getReadAloudModel().onNodeWillBeDeleted?.(paragraph1.firstChild!);


    // The deleted node isn't returned by getCurrentTextContent.
    assertEquals(
        'I can\'t provide. ', getReadAloudModel().getCurrentTextContent());
  });

  test('moveSpeechBackwards before init returns empty text', () => {
    getReadAloudModel().moveSpeechBackwards();
    assertTextEmpty();
  });


  test('init called multiple times does nothing', async () => {
    const paragraph1 = document.createElement('div');
    paragraph1.textContent = 'Keep a grip and take a deep breath. ';

    const paragraph2 = document.createElement('div');
    const bold = document.createElement('strong');
    bold.textContent = 'And soon we\'ll know what\'s what.  ';

    const textAfterBold = document.createTextNode(
        'Put on a show, rewards will flow, and we\'ll go from there.');
    paragraph2.appendChild(bold);
    paragraph2.appendChild(textAfterBold);

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    assertEquals(
        paragraph1.textContent + '\n',
        getReadAloudModel().getCurrentTextContent());

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    getReadAloudModel().moveSpeechForward();
    assertEquals(bold.textContent, getReadAloudModel().getCurrentTextContent());

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        textAfterBold.textContent, getReadAloudModel().getCurrentTextContent());
  });

  test('moveSpeechBackwards returns expected text', async () => {
    const paragraph1 = document.createElement('div');
    paragraph1.textContent = 'See the line where the sky meets the sea? ';

    const paragraph2 = document.createElement('div');
    paragraph2.textContent = 'It calls me. ';

    const paragraph3 = document.createElement('div');
    paragraph3.textContent = 'And no one knows how far it goes.';

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);
    document.body.appendChild(paragraph3);

    await microtasksFinished();

    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    // Move to the last granularity of the content.
    assertSentenceMatchesEntireSegment(paragraph1.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(paragraph2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(paragraph3.textContent);

    // Assert text is expected while moving backwards.
    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(paragraph2.textContent);

    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(paragraph1.textContent);

    // We're at the beginning of the content again, so the first sentence
    // should be retrieved next.
    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(paragraph1.textContent);

    // After navigating previous text, navigating forwards should continue
    // to work as expected.
    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(paragraph2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(paragraph3.textContent);

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText after re-initialization', async () => {
    const paragraph1 = document.createElement('div');
    paragraph1.textContent = 'Breaking my heart one piece at a time. ';

    const paragraph2 = document.createElement('div');
    paragraph2.textContent = 'Well here\'s a piece of mind, yeah. ';

    const paragraph3 = document.createElement('div');
    paragraph3.textContent = 'You don\'t know what it is you to do me.';

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);
    document.body.appendChild(paragraph3);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertSentenceMatchesEntireSegment(paragraph1.textContent);

    // Simulate updating the page text.
    const newParagraph1 = document.createElement('div');
    newParagraph1.textContent = 'Smile on my face to cover my hurt. ';

    const newParagraph2 = document.createElement('div');
    newParagraph2.textContent = 'Spent so much time. ';

    const newParagraph3 = document.createElement('div');
    newParagraph3.textContent = 'But what was it worth? ';

    document.body.removeChild(paragraph1);
    document.body.removeChild(paragraph2);
    document.body.removeChild(paragraph3);

    document.body.appendChild(newParagraph1);
    document.body.appendChild(newParagraph2);
    document.body.appendChild(newParagraph3);

    await microtasksFinished();
    getReadAloudModel().resetModel?.();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    // After re-initialization, the new returned text is correct.
    assertSentenceMatchesEntireSegment(newParagraph1.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph3.textContent);

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('moveSpeechBackwards after re-initialization', async () => {
    const paragraph1 = document.createElement('div');
    paragraph1.textContent = 'This is a sentence. ';

    const paragraph2 = document.createElement('div');
    paragraph2.textContent = 'This is another sentence. ';

    const paragraph3 = document.createElement('div');
    paragraph3.textContent = 'And this is yet another sentence.';

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);
    document.body.appendChild(paragraph3);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertSentenceMatchesEntireSegment(paragraph1.textContent);

    // Simulate updating the page text.
    const newParagraph1 = document.createElement('div');
    newParagraph1.textContent = 'Welcome to the show to the histo-remix. ';

    const newParagraph2 = document.createElement('div');
    newParagraph2.textContent = 'Switching up the flow, as we add the prefix. ';

    const newParagraph3 = document.createElement('div');
    newParagraph3.textContent =
        'Everybody knows that we used to be six wives. ';

    document.body.removeChild(paragraph1);
    document.body.removeChild(paragraph2);
    document.body.removeChild(paragraph3);

    document.body.appendChild(newParagraph1);
    document.body.appendChild(newParagraph2);
    document.body.appendChild(newParagraph3);

    await microtasksFinished();
    getReadAloudModel().resetModel?.();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    // The nodes from the new tree are used.
    // Move to the last node of the content.
    assertSentenceMatchesEntireSegment(newParagraph1.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph3.textContent);

    // Move backwards.
    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(newParagraph2.textContent);

    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(newParagraph1.textContent);

    // We're at the beginning of the content again, so the first sentence
    // should be retrieved next.
    getReadAloudModel().moveSpeechBackwards();
    assertSentenceMatchesEntireSegment(newParagraph1.textContent);

    // After navigating previous text, navigating forwards should continue
    // to work as expected.
    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(newParagraph3.textContent);

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('moveSpeechBackwards when text split across two nodes', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'And I am almost ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'there. ';

    const sentence3 = document.createElement('b');
    sentence3.textContent = 'I am almost there.';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        'And I am almost there. ', getReadAloudModel().getCurrentTextContent());

    // Move to last granularity and then move backwards.
    getReadAloudModel().moveSpeechForward();
    getReadAloudModel().moveSpeechBackwards();
    assertEquals(
        'And I am almost there. ', getReadAloudModel().getCurrentTextContent());
    assertEquals(2, getReadAloudModel().getCurrentTextSegments().length);

    // After moving forward again, the third segment was returned correctly.
    // The third segment was returned correctly after getting the next text.
    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(sentence3.textContent);

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test(
      'moveSpeechBackwards when text split across multiple nodes', async () => {
        const div = document.createElement('div');
        const sentence1 = document.createElement('b');
        sentence1.textContent = 'The wind is howling like this ';

        const sentence2 = document.createElement('a');
        sentence2.textContent = 'swirling storm ';

        const sentence3 = document.createElement('b');
        sentence3.textContent = 'inside.';

        div.appendChild(sentence1);
        div.appendChild(sentence2);
        div.appendChild(sentence3);
        document.body.appendChild(div);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            'The wind is howling like this swirling storm inside.',
            getReadAloudModel().getCurrentTextContent());

        getReadAloudModel().moveSpeechBackwards();
        assertEquals(
            'The wind is howling like this swirling storm inside.',
            getReadAloudModel().getCurrentTextContent());

        // Nodes are empty at the end of the new tree.
        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentText when sentence initially skipped returns expected text',
      async () => {
        const div = document.createElement('div');
        const sentence1 = document.createElement('b');
        sentence1.textContent = 'See the line where the sky meets the sea? ';

        const sentence2 = document.createElement('a');
        sentence2.textContent = 'It calls me. ';

        const sentence3 = document.createElement('b');
        sentence3.textContent = 'And no one knows how far it goes.';

        div.appendChild(sentence1);
        div.appendChild(sentence2);
        div.appendChild(sentence3);
        document.body.appendChild(div);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // Move to third sentence
        getReadAloudModel().moveSpeechForward();
        getReadAloudModel().moveSpeechForward();
        assertSentenceMatchesEntireSegment(sentence3.textContent);

        // Move to second node which was initially skipped
        getReadAloudModel().moveSpeechBackwards();
        assertSentenceMatchesEntireSegment(sentence2.textContent);
      });

  test('getCurrentText after reset starts speech over', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'I\'ve got the wind in my hair. ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'And a gleam in my eyes. ';

    const sentence3 = document.createElement('b');
    sentence3.textContent = 'And an endless horizon. ';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        sentence1.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence2.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // If we init without restarting we should just go to the next sentence.
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence3.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // After reset, we should get the first sentence again.
    getReadAloudModel().resetSpeechToBeginning();
    assertEquals(
        sentence1.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // Speech progresses as expected after a reset.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence2.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence3.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText after restart starts speech over', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'I\'ve got the wind in my hair. ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'And a gleam in my eyes. ';

    const sentence3 = document.createElement('b');
    sentence3.textContent = 'And an endless horizon. ';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        sentence1.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence2.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // If we init without restarting we should just go to the next sentence.
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence3.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // After a restart, current text is empty.
    getReadAloudModel().resetModel?.();
    assertTextEmpty();

    // After init, we should get the first sentence again.
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        sentence1.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    // Speech progresses as expected after a reset.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence2.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence3.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });


  test('getCurrentText with multiple sentences in same node', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'But from up here. The ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'world ';

    const sentence3 = document.createElement('b');
    sentence3.textContent =
        'looks so small. And suddenly life seems so clear. And from up here. ' +
        'You coast past it all. The obstacles just disappear. ';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    // The first segment was returned correctly.
    assertEquals(
        'But from up here.',
        getReadAloudModel().getCurrentTextContent().trim());

    // The second segment was returned correctly, across three nodes.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'The world looks so small.',
        getReadAloudModel().getCurrentTextContent().trim());

    // The third sentence was returned correctly.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'And suddenly life seems so clear.',
        getReadAloudModel().getCurrentTextContent().trim());

    // The fourth sentence was returned correctly.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'And from up here.',
        getReadAloudModel().getCurrentTextContent().trim());

    // The fifth sentence was returned correctly.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'You coast past it all.',
        getReadAloudModel().getCurrentTextContent().trim());

    // The last sentence was returned correctly.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'The obstacles just disappear.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText when sentence split across multiple nodes', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'The wind is howling like this ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'swirling storm ';

    const sentence3 = document.createElement('b');
    sentence3.textContent = 'inside.';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        'The wind is howling like this swirling storm inside.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText when sentence split across two nodes', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'And I am almost ';

    const sentence2 = document.createElement('a');
    sentence2.textContent = 'there. ';

    const sentence3 = document.createElement('b');
    sentence3.textContent = 'I am almost there.';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        'And I am almost there.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'I am almost there.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test(
      'getCurrentTextContent called multiple times returns the same content if moveSpeechForward or moveSpeechBackwards is not called',
      async () => {
        const div = document.createElement('div');
        const sentence1 = document.createElement('strong');
        sentence1.textContent = 'Can\'t we be seventeen? ';

        const sentence2 = document.createElement('i');
        sentence2.textContent = 'That\'s all I want to do.';

        div.appendChild(sentence1);
        div.appendChild(sentence2);
        document.body.appendChild(div);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        for (let i = 0; i < 10; i++) {
          assertSentenceMatchesEntireSegment(sentence1.textContent);
        }

        getReadAloudModel().moveSpeechForward();
        for (let i = 0; i < 10; i++) {
          assertSentenceMatchesEntireSegment(sentence2.textContent);
        }
      });

  test('getCurrentText returns expected text', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('b');
    sentence1.textContent = 'Love showed up at my door yesterday. ';

    const sentence2 = document.createElement('u');
    sentence2.textContent = 'It might sound cheesy, but I wanted her to stay. ';

    const sentence3 = document.createElement('i');
    sentence3.textContent = 'I fell in love with the pizza girl.';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        sentence1.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence2.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        sentence3.textContent.trim(),
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentTextSegments returns expected text', async () => {
    const div = document.createElement('div');
    const sentence1 = document.createElement('h1');
    sentence1.textContent = 'You need space. ';

    const sentence2 = document.createElement('h2');
    sentence2.textContent = 'You need time.';

    const sentence3 = document.createElement('h3');
    sentence3.textContent = 'You take yours, and I\'ll take mine. ';

    div.appendChild(sentence1);
    div.appendChild(sentence2);
    div.appendChild(sentence3);
    document.body.appendChild(div);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertSentenceMatchesEntireSegment(sentence1.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(sentence2.textContent);

    getReadAloudModel().moveSpeechForward();
    assertSentenceMatchesEntireSegment(sentence3.textContent);

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test(
      'moveSpeechBackwards when first initialized returns first sentence',
      async () => {
        const div = document.createElement('div');
        const sentence1 = document.createElement('h1');
        sentence1.textContent = 'This is critical.';

        const sentence2 = document.createElement('h2');
        sentence2.textContent = 'I am feeling helpless.';

        div.appendChild(sentence1);
        div.appendChild(sentence2);
        document.body.appendChild(div);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        // If moveSpeechBackwards is called before moveSpeechForwards, the first
        // sentence should still be returned.
        getReadAloudModel().moveSpeechBackwards();
        assertSentenceMatchesEntireSegment(sentence1.textContent);

        getReadAloudModel().moveSpeechForward();
        assertSentenceMatchesEntireSegment(sentence2.textContent);

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test('getCurrentText when sentence split across paragraph', async () => {
    const paragraph1 = document.createElement('p');
    paragraph1.textContent =
        'Mic check, can you hear me? Gotta know if I\'m coming in ';

    const paragraph2 = document.createElement('p');
    paragraph2.textContent = 'clearly. Static through the speakers, ';

    const paragraph3 = document.createElement('p');
    paragraph3.textContent = 'in a second your heart will be fearless.';

    const paragraph4 = document.createElement('p');
    paragraph4.textContent =
        'Taken for granted. Right now, you can\'t stand it.';

    document.body.appendChild(paragraph1);
    document.body.appendChild(paragraph2);
    document.body.appendChild(paragraph3);
    document.body.appendChild(paragraph4);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals(
        'Mic check, can you hear me?',
        getReadAloudModel().getCurrentTextContent().trim());

    // Even though "Gotta know if I'm coming in clearly." is a complete
    // sentence, the text is divided across a paragraph, so the line breaks
    // should honored in how the text is segmented.
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'Gotta know if I\'m coming in',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'clearly.', getReadAloudModel().getCurrentTextContent().trim());

    // Honor the line break between "Static through the speakers" and "in a
    // second your heart will be fearless."
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'Static through the speakers,',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'in a second your heart will be fearless.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'Taken for granted.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        'Right now, you can\'t stand it.',
        getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertTextEmpty();
  });

  test('getCurrentText includes ordered list items', async () => {
    const ol = document.createElement('ol');
    const li1 = document.createElement('li');
    li1.textContent = 'Realize numbers are ignored in read aloud.';
    const li2 = document.createElement('li');
    li2.textContent = 'Fix it.';
    const li3 = document.createElement('li');
    li3.textContent = 'Profit';
    ol.appendChild(li1);
    ol.appendChild(li2);
    ol.appendChild(li3);
    document.body.appendChild(ol);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    // The beginning of the list item should start with a newline to ensure
    // the list item isn't incorrectly grouped with the previous text. This
    // will be ignored by the TTS engine and not spoken.
    assertEquals('\n', getReadAloudModel().getCurrentTextContent());

    getReadAloudModel().moveSpeechForward();
    assertEquals('1.', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        li1.textContent, getReadAloudModel().getCurrentTextContent().trim());

    // Verify the second bullet.
    getReadAloudModel().moveSpeechForward();
    assertEquals('2.', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        li2.textContent, getReadAloudModel().getCurrentTextContent().trim());

    // Verify the third bullet.
    getReadAloudModel().moveSpeechForward();
    assertEquals('3.', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        li3.textContent, getReadAloudModel().getCurrentTextContent().trim());
  });

  test('getCurrentText for ordered list with custom start', async () => {
    const ol = document.createElement('ol');
    ol.start = 100;
    const li1 = document.createElement('li');
    li1.textContent = 'bugs to fix';
    const li2 = document.createElement('li');
    li2.textContent = 'cls to submit';
    ol.appendChild(li1);
    ol.appendChild(li2);
    document.body.appendChild(ol);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals('\n', getReadAloudModel().getCurrentTextContent());
    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '100. bugs to fix', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '101. cls to submit',
        getReadAloudModel().getCurrentTextContent().trim());
  });

  test('getCurrentText for ordered list with custom values', async () => {
    const ol = document.createElement('ol');
    const li1 = document.createElement('li');
    li1.textContent = 'golden rings';
    li1.value = 5;
    const li2 = document.createElement('li');
    li2.textContent = 'calling birds';
    li2.value = 4;
    const li3 = document.createElement('li');
    li3.textContent = 'french hens';
    li3.value = 3;
    const li4 = document.createElement('li');
    li4.textContent = 'turtle doves';
    li4.value = 2;
    ol.appendChild(li1);
    ol.appendChild(li2);
    ol.appendChild(li3);
    ol.appendChild(li4);
    document.body.appendChild(ol);

    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);
    assertEquals('\n', getReadAloudModel().getCurrentTextContent());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '5. golden rings', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '4. calling birds', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '3. french hens', getReadAloudModel().getCurrentTextContent().trim());

    getReadAloudModel().moveSpeechForward();
    assertEquals(
        '2. turtle doves', getReadAloudModel().getCurrentTextContent().trim());
  });

  test(
      'getCurrentText when sentence split across paragraph without paragraph tags',
      async () => {
        const paragraph1 = document.createElement('b');
        paragraph1.textContent = 'You\'d never get away\n';

        const paragraph2 = document.createElement('b');
        paragraph2.textContent = 'with all this in a play\n';

        const paragraph3 = document.createElement('b');
        paragraph3.textContent = ', but if it\'s loudly sung...';

        document.body.appendChild(paragraph1);
        document.body.appendChild(paragraph2);
        document.body.appendChild(paragraph3);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            paragraph1.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertEquals(
            paragraph2.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertEquals(
            paragraph3.textContent.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentText does not filter out whitespace between nodes',
      async () => {
        const teenLink = document.createElement('a');
        const p = document.createElement('p');
        teenLink.textContent = 'teen';
        p.appendChild(teenLink);

        const whitespaceSpan = document.createElement('span');
        whitespaceSpan.textContent = ' ';
        p.appendChild(whitespaceSpan);

        const satiricalLink = document.createElement('a');
        satiricalLink.textContent = 'satirical';
        p.appendChild(satiricalLink);

        const whitespaceSpan2 = document.createElement('span');
        whitespaceSpan2.textContent = ' ';
        p.appendChild(whitespaceSpan2);

        const crimeFilmLink = document.createElement('a');
        crimeFilmLink.textContent = 'crime film';
        p.appendChild(crimeFilmLink);

        document.body.appendChild(p);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        // If whitespace were filtered out, this would be
        // "teensatiricalcrime film"
        assertEquals(
            'teen satirical crime film',
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments superscript combined with current segment',
      async () => {
        const div = document.createElement('div');
        const sentence1 = document.createElement('b');
        sentence1.textContent = 'And I am almost there.';

        const sentence2 = document.createElement('sup');
        sentence2.textContent = '2';

        div.appendChild(sentence1);
        div.appendChild(sentence2);
        document.body.appendChild(div);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            sentence1.textContent + sentence2.textContent,
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments superscript combined with preceding sentence instead of succeeding sentence',
      async () => {
        // Create a container element to hold the simplified structure
        const simplifiedContainer = document.createElement('p');

        // Text before the citation
        simplifiedContainer.appendChild(
            document.createTextNode('I\'m coming!'));

        // <sup><a><span>[</span>b<span>]</span></a></sup>
        const citation = createLinkedCitationSuperscript('b');
        simplifiedContainer.appendChild(citation);

        // Text after the citation.
        simplifiedContainer.appendChild(
            document.createTextNode(' Wait for me.'));

        document.body.appendChild(simplifiedContainer);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            'I\'m coming![b]',
            getReadAloudModel().getCurrentTextContent().trim());
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            'Wait for me.', getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments superscript combined with preceding sentence with multiple superscripts',
      async () => {
        // Create a container element to hold the simplified structure
        const simplifiedContainer = document.createElement('p');

        // Text before the citation
        simplifiedContainer.appendChild(
            document.createTextNode('Wait for me, I\'m coming.'));

        // <sup><a><span>[</span>7<span>]</span></a></sup>
        const citation7 = createLinkedCitationSuperscript('7');
        simplifiedContainer.appendChild(citation7);

        // <sup><a><span>[</span>8<span>]</span></a></sup>
        const citation8 = createLinkedCitationSuperscript('8');
        simplifiedContainer.appendChild(citation8);

        // Text after the citation.
        simplifiedContainer.appendChild(
            document.createTextNode(' Show the way so we can see.'));

        document.body.appendChild(simplifiedContainer);

        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            'Wait for me, I\'m coming.[7][8]',
            getReadAloudModel().getCurrentTextContent().trim());
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            'Show the way so we can see.',
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments all superscript nodes included in current sentence',
      async () => {
        // Create a container element to hold the simplified structure
        const paragraph = document.createElement('p');

        // Text before the citation
        paragraph.appendChild(document.createTextNode('Doubt comes in.'));

        const superscript = document.createElement('sup');
        superscript.appendChild(document.createTextNode('['));
        superscript.appendChild(document.createTextNode('3'));
        superscript.appendChild(document.createTextNode(']'));
        paragraph.appendChild(superscript);

        document.body.appendChild(paragraph);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // The first sentence and its superscript are returned as one segment.
        assertEquals(
            'Doubt comes in.[3]',
            getReadAloudModel().getCurrentTextContent().trim());


        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments superscript with brackets included in current sentence',
      async () => {
        // Create a container element to hold the simplified structure
        const paragraph = document.createElement('p');

        // Text before the citation
        paragraph.appendChild(document.createTextNode('Doubt comes in.'));

        const superscript = document.createElement('sup');
        superscript.appendChild(document.createTextNode('[2]'));
        paragraph.appendChild(superscript);

        document.body.appendChild(paragraph);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // The first sentence and its superscript are returned as one segment.
        assertEquals(
            'Doubt comes in.[2]',
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });

  test(
      'getCurrentTextSegments superscript included when entire node and more text is after superscript',
      async () => {
        // Create a container element to hold the simplified structure
        const paragraph = document.createElement('p');

        // Text before the citation
        paragraph.appendChild(
            document.createTextNode('And I am almost there.'));

        const superscript = document.createElement('sup');
        superscript.appendChild(document.createTextNode('['));
        superscript.appendChild(document.createTextNode('23'));
        superscript.appendChild(document.createTextNode(']'));
        paragraph.appendChild(superscript);

        // Text after the citation
        paragraph.appendChild(
            document.createTextNode('People gon\' come here from everywhere.'));

        document.body.appendChild(paragraph);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // The first sentence and its superscript are returned as one segment.
        assertEquals(
            'And I am almost there.[23]',
            getReadAloudModel().getCurrentTextContent().trim());

        // The next sentence and is returned on its own.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            'People gon\' come here from everywhere.',
            getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechForward();
        assertTextEmpty();
      });


  test(
      'getHighlightForCurrentSegmentIndex returns correct nodess', async () => {
        const paragraph = document.createElement('p');
        const child = document.createTextNode('I\'m crossing the line!');
        paragraph.appendChild(child);
        document.body.appendChild(paragraph);
        await microtasksFinished();

        // Before there are any processed granularities,
        // getHighlightForCurrentSegmentIndex should be empty.
        assertEquals(
            0,
            getReadAloudModel()
                .getHighlightForCurrentSegmentIndex(0, false)
                .length);
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        expectHighlightAtIndexMatches(0, [{node: child, start: 0, length: 3}]);
        expectHighlightAtIndexMatches(3, [{node: child, start: 3, length: 9}]);
        expectHighlightAtIndexMatches(7, [{node: child, start: 7, length: 5}]);
        expectHighlightAtIndexMatches(
            child.textContent.length - 2,
            [{node: child, start: 20, length: 1}]);
        expectHighlightAtIndexMatchesEmpty(child.textContent.length);
      });


  test(
      ' getHighlightForCurrentSegmentIndex sentence spans multiple nodes returns correct node',
      async () => {
        const paragraph = document.createElement('p');
        // Text indices:                      0123456789012345678901234567890
        const child1 = document.createTextNode('Never feel heavy ');
        const child2 = document.createTextNode('or earthbound, ');
        const child3 =
            document.createTextNode('no worries or doubts interfere.');
        paragraph.appendChild(child1);
        paragraph.appendChild(child2);
        paragraph.appendChild(child3);
        document.body.appendChild(paragraph);
        await microtasksFinished();

        // Before there are any processed granularities,
        // getHighlightForCurrentSegmentIndex should be empty.
        assertEquals(
            0,
            getReadAloudModel()
                .getHighlightForCurrentSegmentIndex(0, false)
                .length);
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // Spot check that indices 0->sentence1.length() map to the first node
        // id.
        let baseLength: number = child1.textContent.length;
        expectHighlightAtIndexMatches(0, [{node: child1, start: 0, length: 5}]);
        expectHighlightAtIndexMatches(7, [{node: child1, start: 7, length: 3}]);

        // The trailing whitespace is segmented with the next word.
        expectHighlightAtIndexMatches(baseLength - 1, [
          {node: child1, start: 16, length: 1},
          {node: child2, start: 0, length: 2},
        ]);

        // Spot check that indices in sentence 2 map to the second node id.
        baseLength += child2.textContent.length;
        expectHighlightAtIndexMatches(
            child1.textContent.length + 1,
            [{node: child2, start: 1, length: 1}]);
        expectHighlightAtIndexMatches(
            26, [{node: child2, start: 9, length: 4}]);
        expectHighlightAtIndexMatches(baseLength - 1, [
          {node: child2, start: 14, length: 1},
          {node: child3, start: 0, length: 2},
        ]);
        expectHighlightAtIndexMatches(
            baseLength, [{node: child3, start: 0, length: 2}]);

        // Spot check that indices in sentence 3 map to the third node id.
        baseLength += child3.textContent.length;
        expectHighlightAtIndexMatches(
            child1.textContent.length + child2.textContent.length + 1,
            [{node: child3, start: 1, length: 1}]);
        expectHighlightAtIndexMatches(
            40, [{node: child3, start: 8, length: 2}]);

        // Out-of-bounds nodes return an empty array.
        expectHighlightAtIndexMatchesEmpty(baseLength - 1);
        expectHighlightAtIndexMatchesEmpty(535);
        expectHighlightAtIndexMatchesEmpty(-10);
      });

  test('getCurrentTextSegments returns correct segments', async () => {
    const p = document.createElement('p');
    p.textContent = 'I broke into a million pieces';
    document.body.appendChild(p);
    await microtasksFinished();
    getReadAloudModel().init(ReadAloudNode.create(document.body)!);

    const segments = getReadAloudModel().getCurrentTextSegments();
    assertEquals(1, segments.length);
    assertEquals(p.firstChild, segments[0]!.node.domNode());
    assertEquals(0, segments[0]!.start);
    assertEquals(p.textContent.length, segments[0]!.length);
  });

  test(
      'getCurrentTextSegments sentence spans multiple nodes returns correct segments',
      async () => {
        const div = document.createElement('div');
        const sentence1 = document.createTextNode('and I can\'t go back, ');
        const sentence2 =
            document.createTextNode('But now I\'m seeing all the beauty ');
        const sentence3 = document.createTextNode('in the broken glass.');
        div.appendChild(sentence1);
        div.appendChild(sentence2);
        div.appendChild(sentence3);
        document.body.appendChild(div);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        const segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(3, segments.length);
        assertEquals(sentence1, segments[0]!.node.domNode());
        assertEquals(0, segments[0]!.start);
        assertEquals(sentence1.textContent.length, segments[0]!.length);
        assertEquals(sentence2, segments[1]!.node.domNode());
        assertEquals(0, segments[1]!.start);
        assertEquals(sentence2.textContent.length, segments[1]!.length);
        assertEquals(sentence3, segments[2]!.node.domNode());
        assertEquals(0, segments[2]!.start);
        assertEquals(sentence3.textContent.length, segments[2]!.length);
      });

  test(
      'getCurrentTextSegments node spans multiple sentences returns correct segments',
      async () => {
        const segment1 = 'The scars are part of me! ';
        const segment2 = 'Darkness and harmony. ';
        const segment3 = 'My voice without the lies. ';
        const segment4 = 'This is what it sounds ';
        const node1Text = segment1 + segment2 + segment3 + segment4;
        const node2Text = 'like.';
        const div = document.createElement('div');
        const node1 = document.createTextNode(node1Text);
        div.appendChild(node1);
        const node2 = document.createTextNode(node2Text);
        div.appendChild(node2);
        document.body.appendChild(div);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // Sentence 1
        assertEquals(
            segment1.trim(),
            getReadAloudModel().getCurrentTextContent().trim());
        let segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(1, segments.length);
        assertEquals(node1, segments[0]!.node.domNode());
        assertEquals(0, segments[0]!.start);
        assertEquals(segment1.length, segments[0]!.length);

        // Sentence 2
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            segment2.trim(),
            getReadAloudModel().getCurrentTextContent().trim());
        segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(1, segments.length);
        assertEquals(node1, segments[0]!.node.domNode());
        assertEquals(segment1.length, segments[0]!.start);
        assertEquals(segment2.length, segments[0]!.length);

        // Sentence 3
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            segment3.trim(),
            getReadAloudModel().getCurrentTextContent().trim());
        segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(1, segments.length);
        assertEquals(node1, segments[0]!.node.domNode());
        assertEquals(segment1.length + segment2.length, segments[0]!.start);
        assertEquals(segment3.length, segments[0]!.length);

        // Sentence 4
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            (segment4 + node2Text).trim(),
            getReadAloudModel().getCurrentTextContent().trim());
        segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(2, segments.length);
        assertEquals(node1, segments[0]!.node.domNode());
        assertEquals(
            segment1.length + segment2.length + segment3.length,
            segments[0]!.start);
        assertEquals(segment4.length, segments[0]!.length);
        assertEquals(node2, segments[1]!.node.domNode());
        assertEquals(0, segments[1]!.start);
        assertEquals(node2Text.length, segments[1]!.length);
      });

  test(
      'getCurrentTextSegments after previous returns correct nodes',
      async () => {
        const sentence1 =
            'Why did I cover up the colors stuck inside my head? ';
        const sentence2 = 'I should\'ve let the jagged edges.';
        const sentence3 = 'meet the light instead.';
        const p1 = document.createElement('p');
        p1.textContent = sentence1;
        const p2 = document.createElement('p');
        p2.textContent = sentence2;
        const p3 = document.createElement('p');
        p3.textContent = sentence3;
        document.body.appendChild(p1);
        document.body.appendChild(p2);
        document.body.appendChild(p3);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        getReadAloudModel().moveSpeechForward();
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            sentence3, getReadAloudModel().getCurrentTextContent().trim());

        getReadAloudModel().moveSpeechBackwards();
        assertEquals(
            sentence2, getReadAloudModel().getCurrentTextContent().trim());
        const segments = getReadAloudModel().getCurrentTextSegments();
        assertEquals(1, segments.length);
        assertEquals(p2.firstChild, segments[0]!.node.domNode());
        assertEquals(0, segments[0]!.start);
        assertEquals(sentence2.length, segments[0]!.length);
      });

  test(
      'getHighlightForCurrentSegmentIndex after moving forward returns correct nodes',
      async () => {
        const sentence1 = 'Never feel heavy or earthbound. ';
        const sentence2 = 'No worries or doubts ';
        const sentence3 = 'interfere.';
        const paragraph = document.createElement('p');
        const child1 = document.createTextNode(sentence1);
        const child2 = document.createTextNode(sentence2);
        const child3 = document.createTextNode(sentence3);
        paragraph.appendChild(child1);
        paragraph.appendChild(child2);
        paragraph.appendChild(child3);
        document.body.appendChild(paragraph);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // Spot check that indices 0->sentence1.length() map to the first node
        // id.
        expectHighlightAtIndexMatches(0, [{node: child1, start: 0, length: 5}]);
        expectHighlightAtIndexMatches(7, [{node: child1, start: 7, length: 3}]);
        expectHighlightAtIndexMatchesEmpty(sentence1.length - 1);
        expectHighlightAtIndexMatchesEmpty(sentence1.length);

        // Move to the next granularity.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            (sentence2 + sentence3).trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // Spot check that indices in sentence 2 map to the second node id.
        expectHighlightAtIndexMatches(0, [{node: child2, start: 0, length: 2}]);
        expectHighlightAtIndexMatches(7, [{node: child2, start: 7, length: 3}]);
        expectHighlightAtIndexMatches(
            sentence2.length, [{node: child3, start: 0, length: 9}]);

        // Spot check that indices in sentence 3 map to the third node id.
        expectHighlightAtIndexMatches(
            sentence2.length + 1, [{node: child3, start: 1, length: 8}]);
        expectHighlightAtIndexMatches(
            27, [{node: child3, start: 6, length: 3}]);
        expectHighlightAtIndexMatchesEmpty(
            sentence2.length + sentence3.length - 1);

        // Out-of-bounds nodes return invalid.
        expectHighlightAtIndexMatchesEmpty(
            sentence2.length + sentence3.length + 1);
      });

  test(
      'getHighlightForCurrentSegmentIndex after backwards returns correct highlight',
      async () => {
        const sentence1 = 'There\'s nothing but you ';
        const sentence2 = 'looking down on the view from up here. ';
        const sentence3 = 'Stretch out with the wind behind you.';
        const paragraph = document.createElement('p');
        const child1 = document.createTextNode(sentence1);
        const child2 = document.createTextNode(sentence2);
        const child3 = document.createTextNode(sentence3);
        paragraph.appendChild(child1);
        paragraph.appendChild(child2);
        paragraph.appendChild(child3);

        document.body.appendChild(paragraph);
        await microtasksFinished();

        // Before there are any processed granularities, there should be no
        // highlights.
        expectHighlightAtIndexMatchesEmpty(1);
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        getReadAloudModel().moveSpeechForward();
        // Spot check that indices 0->sentence3.length() map to the third node
        // id.
        expectHighlightAtIndexMatches(0, [{node: child3, start: 0, length: 7}]);
        expectHighlightAtIndexMatches(7, [{node: child3, start: 7, length: 4}]);
        expectHighlightAtIndexMatchesEmpty(sentence3.length - 1);

        getReadAloudModel().moveSpeechBackwards();
        assertEquals(
            (sentence1 + sentence2).trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // Spot check that indices in sentence 1 map to the first node id.
        expectHighlightAtIndexMatches(0, [{node: child1, start: 0, length: 7}]);
        expectHighlightAtIndexMatches(6, [{node: child1, start: 6, length: 1}]);
        expectHighlightAtIndexMatches(sentence1.length - 1, [
          {node: child1, start: 23, length: 1},
          {node: child2, start: 0, length: 7},
        ]);

        // Spot check that indices in sentence 2 map to the second node id.
        expectHighlightAtIndexMatches(
            sentence1.length + 1, [{node: child2, start: 1, length: 6}]);
        expectHighlightAtIndexMatches(
            27, [{node: child2, start: 3, length: 4}]);
        expectHighlightAtIndexMatchesEmpty(
            sentence1.length + sentence2.length - 1);

        // Out-of-bounds nodes return invalid.
        expectHighlightAtIndexMatchesEmpty(
            sentence1.length + sentence2.length + 1);
      });

  test(
      'getHighlightForCurrentSegmentIndex multinode words returns correct length',
      async () => {
        const word1 = 'Stretch ';
        const word2 = 'out ';
        const word3 = 'with ';
        const word4 = 'the ';
        const word5 = 'wind ';
        const word6 = 'beh';
        const word7 = 'ind ';
        const word8 = 'you.';
        const sentence1 = word1 + word2 + word3 + word4 + word5 + word6;
        const sentence2 = word7 + word8;
        const p = document.createElement('p');
        const node1 = document.createTextNode(sentence1);
        const node2 = document.createTextNode(sentence2);
        p.appendChild(node1);
        p.appendChild(node2);
        document.body.appendChild(p);
        await microtasksFinished();

        getReadAloudModel().init(ReadAloudNode.create(document.body)!);
        assertEquals(
            'Stretch out with the wind behind you.',
            getReadAloudModel().getCurrentTextContent().trim());

        // Throughout first word.
        expectHighlightAtIndexMatches(0, [{node: node1, start: 0, length: 7}]);
        expectHighlightAtIndexMatches(2, [{node: node1, start: 2, length: 5}]);
        expectHighlightAtIndexMatches(
            word1.length - 2, [{node: node1, start: 6, length: 1}]);

        // Throughout third word.
        const thirdWordIndex: number = sentence1.indexOf(word3);
        expectHighlightAtIndexMatches(
            thirdWordIndex, [{node: node1, start: 12, length: 4}]);
        expectHighlightAtIndexMatches(
            thirdWordIndex + 2, [{node: node1, start: 14, length: 2}]);

        // Words split across node boundaries
        const sixthWordIndex: number = sentence1.indexOf(word6);
        expectHighlightAtIndexMatches(sixthWordIndex, [
          {node: node1, start: 26, length: 3},
          {node: node2, start: 0, length: 3},
        ]);
        expectHighlightAtIndexMatches(sixthWordIndex + 2, [
          {node: node1, start: 28, length: 1},
          {node: node2, start: 0, length: 3},
        ]);

        const seventhWordIndex: number = sentence1.length;
        expectHighlightAtIndexMatches(
            seventhWordIndex, [{node: node2, start: 0, length: 3}]);
        expectHighlightAtIndexMatches(
            seventhWordIndex + 2, [{node: node2, start: 2, length: 1}]);

        const lastWordIndex = sentence1.length + sentence2.indexOf(word8);
        expectHighlightAtIndexMatches(
            lastWordIndex, [{node: node2, start: 4, length: 3}]);
        expectHighlightAtIndexMatches(
            lastWordIndex + 2, [{node: node2, start: 6, length: 1}]);

        // Boundary testing.
        expectHighlightAtIndexMatchesEmpty(-5);
        expectHighlightAtIndexMatchesEmpty(sentence1.length + sentence2.length);
        expectHighlightAtIndexMatchesEmpty(
            sentence1.length + sentence2.length + 1);
      });

  test(
      'getHighlightForCurrentSegmentIndex node spans multiple sentences returns correct nodes',
      async () => {
        const segment1 = 'I\'m taking what\'s mine! ';
        const segment2 = 'Every drop, every smidge. ';
        const segment3 = 'If I\'m burning a bridge, let it burn. ';
        const segment4 = 'But I\'m crossing the ';
        const node1Text = segment1 + segment2 + segment3 + segment4;
        const node2Text = 'line.';

        const div = document.createElement('div');
        const node1 = document.createTextNode(node1Text);
        div.appendChild(node1);
        const node2 = document.createTextNode(node2Text);
        div.appendChild(node2);
        document.body.appendChild(div);
        await microtasksFinished();
        getReadAloudModel().init(ReadAloudNode.create(document.body)!);

        // First sentence
        assertEquals(
            segment1.trim(),
            getReadAloudModel().getCurrentTextContent().trim());
        expectHighlightAtIndexMatches(0, [{node: node1, start: 0, length: 3}]);
        expectHighlightAtIndexMatches(6, [{node: node1, start: 6, length: 4}]);
        expectHighlightAtIndexMatches(
            15, [{node: node1, start: 15, length: 2}]);
        expectHighlightAtIndexMatchesEmpty(segment1.length - 1);
        expectHighlightAtIndexMatchesEmpty(segment1.length);

        // Move to segment 2.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            segment2.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // For the second segment, the boundary index will have reset for the
        // new speech segment. The correct highlight start index is the index
        // that the boundary index within the segment corresponds to within the
        // node.
        let baseLength: number = segment1.length;
        expectHighlightAtIndexMatches(
            0, [{node: node1, start: baseLength, length: 5}]);
        expectHighlightAtIndexMatches(
            10, [{node: node1, start: baseLength + 10, length: 7}]);
        expectHighlightAtIndexMatches(
            13, [{node: node1, start: baseLength + 13, length: 4}]);

        baseLength += segment2.length;
        expectHighlightAtIndexMatchesEmpty(segment2.length - 1);
        expectHighlightAtIndexMatchesEmpty(segment1.length + segment2.length);

        // Move to segment 3.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            segment3.trim(),
            getReadAloudModel().getCurrentTextContent().trim());

        // For the third segment, the boundary index will have reset for the new
        // speech segment. The correct highlight start index is the index that
        // the boundary index within the segment corresponds to within the node.
        expectHighlightAtIndexMatches(
            0, [{node: node1, start: baseLength, length: 2}]);
        expectHighlightAtIndexMatches(
            9, [{node: node1, start: baseLength + 9, length: 5}]);
        expectHighlightAtIndexMatches(
            13, [{node: node1, start: baseLength + 13, length: 1}]);
        expectHighlightAtIndexMatchesEmpty(segment3.length - 1);
        expectHighlightAtIndexMatchesEmpty(baseLength + segment3.length - 1);

        // Move to segment 4.
        getReadAloudModel().moveSpeechForward();
        assertEquals(
            segment4 + node2Text,
            getReadAloudModel().getCurrentTextContent().trim());

        // For the fourth segment, there are two nodes. For the first node,
        // the correct highlight start corresponds to the index within the first
        // node.
        baseLength += segment3.length;
        expectHighlightAtIndexMatches(
            0, [{node: node1, start: baseLength, length: 3}]);
        expectHighlightAtIndexMatches(
            2, [{node: node1, start: baseLength + 2, length: 1}]);
        expectHighlightAtIndexMatches(
            8, [{node: node1, start: baseLength + 8, length: 8}]);
        expectHighlightAtIndexMatches(segment4.length - 1, [
          {node: node1, start: baseLength + segment4.length - 1, length: 1},
          {node: node2, start: 0, length: 4},
        ]);


        // For the second node, the highlight index corresponds to the position
        // within the second node.
        expectHighlightAtIndexMatches(
            segment4.length, [{node: node2, start: 0, length: 4}]);
        expectHighlightAtIndexMatches(
            segment4.length + 2, [{node: node2, start: 2, length: 2}]);
        expectHighlightAtIndexMatches(
            (segment4 + node2Text).length - 2,
            [{node: node2, start: node2Text.length - 2, length: 1}]);
        expectHighlightAtIndexMatchesEmpty((segment4 + node2Text).length - 1);
        expectHighlightAtIndexMatchesEmpty((segment4 + node2Text).length);
      });
});
