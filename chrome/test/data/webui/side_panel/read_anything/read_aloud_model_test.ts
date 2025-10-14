// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {getReadAloudModel, ReadAloudNode, setInstance} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome-untrusted://webui-test/test_util.js';


suite('ReadAloudModel', () => {
  setup(() => {
    // Clearing the DOM should always be done first.
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    setInstance(null);
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
});
