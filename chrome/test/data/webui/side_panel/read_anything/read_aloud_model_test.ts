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

  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setInstance(null);
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
});
