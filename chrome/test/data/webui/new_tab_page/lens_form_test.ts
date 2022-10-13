// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://new-tab-page/new_tab_page.js';

import {LensErrorType, LensFormElement} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('LensFormTest', () => {
  let lensForm: LensFormElement;

  let fileFormSubmitted = false;
  let lastError: LensErrorType|null = null;
  let loading = false;

  setup(() => {
    lensForm = document.createElement('ntp-lens-form');
    document.body.appendChild(lensForm);

    lensForm.$.fileForm.submit = () => {
      fileFormSubmitted = true;
    };

    lensForm.addEventListener('error', (e: Event) => {
      const event = e as CustomEvent<LensErrorType>;
      lastError = event.detail;
    });

    lensForm.addEventListener('loading', () => {
      loading = true;
    });
  });

  teardown(() => {
    fileFormSubmitted = false;
    lastError = null;
    loading = false;
  });

  test('select png files should submit file form', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    // Assert.
    assertTrue(fileFormSubmitted);
    assertEquals(null, lastError);
    assertTrue(loading);
  });

  test(
      'select multiple files should fail with multiple files error',
      async () => {
        // Arrange.
        const file1 = new File([], 'file-1.png', {type: 'image/png'});
        const file2 = new File([], 'file-2.png', {type: 'image/png'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(file1);
        dataTransfer.items.add(file2);

        // Act.
        dispatchFileInputChangeWithDataTransfer(dataTransfer);

        // Assert.
        assertFalse(fileFormSubmitted);
        assertEquals(LensErrorType.MULTIPLE_FILES, lastError);
        assertFalse(loading);
      });

  test('select no files should fail with no files error', async () => {
    // Arrange.
    const dataTransfer = new DataTransfer();

    // Act.
    dispatchFileInputChangeWithDataTransfer(dataTransfer);

    // Assert.
    assertFalse(fileFormSubmitted);
    assertEquals(LensErrorType.NO_FILE, lastError);
    assertFalse(loading);
  });

  test(
      'select unsupported file type should fail with file type error',
      async () => {
        // Arrange.
        const file = new File([], 'file-name.pdf', {type: 'image/pdf'});

        // Act.
        dispatchFileInputChange(file);

        // Assert.
        assertFalse(fileFormSubmitted);
        assertEquals(LensErrorType.FILE_TYPE, lastError);
        assertFalse(loading);
      });

  function dispatchFileInputChangeWithDataTransfer(dataTransfer: DataTransfer) {
    lensForm.$.fileInput.files = dataTransfer.files;
    lensForm.$.fileInput.dispatchEvent(new Event('change'));
  }

  function dispatchFileInputChange(file: File) {
    const dataTransfer = new DataTransfer();
    dataTransfer.items.add(file);
    dispatchFileInputChangeWithDataTransfer(dataTransfer);
  }
});
