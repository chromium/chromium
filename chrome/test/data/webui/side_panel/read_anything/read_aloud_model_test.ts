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
});
