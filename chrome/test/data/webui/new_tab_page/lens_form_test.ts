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
  let urlFormSubmitted = false;
  let lastError: LensErrorType|null = null;
  let loading = false;

  setup(() => {
    lensForm = document.createElement('ntp-lens-form');
    document.body.appendChild(lensForm);

    lensForm.$.fileForm.submit = () => {
      fileFormSubmitted = true;
    };

    lensForm.$.urlForm.submit = () => {
      urlFormSubmitted = true;
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
    urlFormSubmitted = false;
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

  test('submit file should set entrypoint parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const ep = action.searchParams.get('ep');
    assertEquals('cntpubb', ep);
  });

  test('submit file should set language parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const hl = action.searchParams.get('hl');
    assertEquals('en-US', hl);
  });

  test('submit file should set start time parameter', async () => {
    // Arrange.
    Date.now = () => 1001;
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const st = action.searchParams.get('st');
    assertEquals('1001', st);
  });

  test('submit url with valid http should submit', async () => {
    // Arrange.
    const url = 'http://www.example.com/dog.jpg';

    // Act.
    lensForm.submitUrl(url);

    // Assert.
    assertTrue(urlFormSubmitted);
    assertTrue(loading);
  });

  test('submit url with valid https should submit', async () => {
    // Arrange.
    const url = 'https://www.example.com/dog.jpg';

    // Act.
    lensForm.submitUrl(url);

    // Assert.
    assertTrue(urlFormSubmitted);
    assertTrue(loading);
  });

  test(
      'submit url with empty scheme should fail with invalid scheme error',
      async () => {
        // Arrange.
        const url = 'www.example.com/dog.jpg';

        // Act.
        lensForm.submitUrl(url);

        // Assert.
        assertFalse(urlFormSubmitted);
        assertEquals(LensErrorType.INVALID_SCHEME, lastError);
      });

  test(
      'submit url with invalid scheme should fail with invalid scheme error',
      async () => {
        // Arrange.
        const url = 'file://www.example.com/dog.jpg';

        // Act.
        lensForm.submitUrl(url);

        // Assert.
        assertFalse(urlFormSubmitted);
        assertEquals(LensErrorType.INVALID_SCHEME, lastError);
      });

  test('submit invalid url should fail with invalid url error', async () => {
    // Arrange.
    const url = 'http://www.example.com/\uD800.jpg';

    // Act.
    lensForm.submitUrl(url);

    // Assert.
    assertFalse(urlFormSubmitted);
    assertEquals(LensErrorType.INVALID_URL, lastError);
  });

  test('submit long url should fail with length too great error', async () => {
    // Arrange.
    let longString = 'http://www.example.com/dog.jpg?a=';
    for (let i = 0; i < 2000; i++) {
      longString += 'x';
    }

    // Act.
    lensForm.submitUrl(longString);

    // Assert.
    assertFalse(urlFormSubmitted);
    assertEquals(LensErrorType.LENGTH_TOO_GREAT, lastError);
  });

  test('submit url should set entrypoint parameter', async () => {
    // Arrange.
    const input =
        lensForm.$.urlForm.children.namedItem('ep') as HTMLInputElement;

    // Assert.
    assertEquals('cntpubu', input.value);
  });

  test('submit url should set language parameter', async () => {
    // Arrange.
    const input =
        lensForm.$.urlForm.children.namedItem('hl') as HTMLInputElement;

    // Assert.
    assertEquals('en-US', input.value);
  });

  test('submit url should set start time parameter', async () => {
    // Arrange.
    Date.now = () => 1001;
    const input =
        lensForm.$.urlForm.children.namedItem('st') as HTMLInputElement;

    // Act.
    const url = 'https://www.example.com/dog.jpg';
    lensForm.submitUrl(url);

    // Assert.
    assertEquals('1001', input.value);
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
