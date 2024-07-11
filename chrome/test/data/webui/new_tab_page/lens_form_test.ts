// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {LensFormElement} from 'chrome://new-tab-page/lazy_load.js';
import {LensErrorType, LensSubmitType} from 'chrome://new-tab-page/lazy_load.js';
import {assertEquals, assertFalse, assertGT, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('LensFormTest', () => {
  let lensForm: LensFormElement;

  let fileFormSubmitted = false;
  let urlFormSubmitted = false;
  let lastError: LensErrorType|null = null;
  let lastSubmit: LensSubmitType|null = null;

  function loadingHandler(e: Event) {
    const event = e as CustomEvent<LensSubmitType>;
    lastSubmit = event.detail;
  }

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
  });

  teardown(() => {
    fileFormSubmitted = false;
    urlFormSubmitted = false;
    lastError = null;
    lastSubmit = null;
  });

  test('select png files should submit file form', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

    // Assert.
    assertTrue(fileFormSubmitted);
    assertEquals(null, lastError);
    assertEquals(LensSubmitType.FILE, lastSubmit);
  });

  test(
      'select multiple files should fail with multiple files error',
      async () => {
        // Arrange.
        lensForm.addEventListener('loading', loadingHandler);
        const file1 = new File([], 'file-1.png', {type: 'image/png'});
        const file2 = new File([], 'file-2.png', {type: 'image/png'});
        const dataTransfer = new DataTransfer();
        dataTransfer.items.add(file1);
        dataTransfer.items.add(file2);

        // Act.
        dispatchFileInputChangeWithDataTransfer(dataTransfer);
        await microtasksFinished();

        // Assert.
        assertFalse(fileFormSubmitted);
        assertEquals(LensErrorType.MULTIPLE_FILES, lastError);
        assertEquals(null, lastSubmit);
      });

  test('select no files should fail with no files error', async () => {
    // Arrange.
    lensForm.addEventListener('loading', loadingHandler);
    const dataTransfer = new DataTransfer();

    // Act.
    dispatchFileInputChangeWithDataTransfer(dataTransfer);
    await microtasksFinished();

    // Assert.
    assertFalse(fileFormSubmitted);
    assertEquals(LensErrorType.NO_FILE, lastError);
    assertEquals(null, lastSubmit);
  });

  test(
      'select unsupported file type should fail with file type error',
      async () => {
        // Arrange.
        lensForm.addEventListener('loading', loadingHandler);
        const file = new File([], 'file-name.pdf', {type: 'image/pdf'});

        // Act.
        dispatchFileInputChange(file);
        await microtasksFinished();

        // Assert.
        assertFalse(fileFormSubmitted);
        assertEquals(LensErrorType.FILE_TYPE, lastError);
        assertEquals(null, lastSubmit);
      });

  test('submit file should set entrypoint parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const ep = action.searchParams.get('ep');
    assertEquals('cntpubb', ep);
  });

  test('submit file should set rendering environment parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const re = action.searchParams.get('re');
    assertEquals('df', re);
  });

  test('submit file should set surface parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const s = action.searchParams.get('s');
    assertEquals('4', s);
  });

  test('submit file should set language parameter', async () => {
    // Arrange.
    const file = new File([], 'file-name.png', {type: 'image/png'});

    // Act.
    dispatchFileInputChange(file);

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

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

    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);

    // Assert.
    const action = new URL(lensForm.$.fileForm.action);
    const st = action.searchParams.get('st');
    assertEquals('1001', st);
  });

  test(
      'submit url should set client data param to a non-empty value',
      async () => {
        // Arrange.
        const file = new File([], 'file-name.png', {type: 'image/png'});

        // Act.
        dispatchFileInputChange(file);

        const e = await eventToPromise('loading', lensForm);
        loadingHandler(e);

        // Assert.
        const action = new URL(lensForm.$.fileForm.action);
        const cd = action.searchParams.get('cd');
        assertGT(cd!.length, 0);
      });


  test('submit url with valid http should submit', async () => {
    // Arrange.
    const url = 'http://www.example.com/dog.jpg';

    // Act.
    lensForm.submitUrl(url);
    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);
    await microtasksFinished();


    // Assert.
    assertTrue(urlFormSubmitted);
    assertEquals(LensSubmitType.URL, lastSubmit);
  });

  test('submit url with valid https should submit', async () => {
    // Arrange.
    const url = 'https://www.example.com/dog.jpg';

    // Act.
    lensForm.submitUrl(url);
    const e = await eventToPromise('loading', lensForm);
    loadingHandler(e);
    await microtasksFinished();

    // Assert.
    assertTrue(urlFormSubmitted);
    assertEquals(LensSubmitType.URL, lastSubmit);
  });

  test(
      'submit url with empty scheme should fail with invalid scheme error',
      async () => {
        // Arrange.
        lensForm.addEventListener('loading', loadingHandler);
        const url = 'www.example.com/dog.jpg';

        // Act.
        lensForm.submitUrl(url);
        await microtasksFinished();

        // Assert.
        assertFalse(urlFormSubmitted);
        assertEquals(LensErrorType.INVALID_SCHEME, lastError);
        assertEquals(null, lastSubmit);
      });

  test(
      'submit url with invalid scheme should fail with invalid scheme error',
      async () => {
        // Arrange.
        lensForm.addEventListener('loading', loadingHandler);
        const url = 'file://www.example.com/dog.jpg';

        // Act.
        lensForm.submitUrl(url);
        await microtasksFinished();

        // Assert.
        assertFalse(urlFormSubmitted);
        assertEquals(LensErrorType.INVALID_SCHEME, lastError);
        assertEquals(null, lastSubmit);
      });

  test('submit invalid url should fail with invalid url error', async () => {
    // Arrange.
    lensForm.addEventListener('loading', loadingHandler);
    const url = 'http://www.example.com/\uD800.jpg';

    // Act.
    lensForm.submitUrl(url);
    await microtasksFinished();

    // Assert.
    assertFalse(urlFormSubmitted);
    assertEquals(LensErrorType.INVALID_URL, lastError);
    assertEquals(null, lastSubmit);
  });

  test('submit long url should fail with length too great error', async () => {
    // Arrange.
    lensForm.addEventListener('loading', loadingHandler);
    let longString = 'http://www.example.com/dog.jpg?a=';
    for (let i = 0; i < 2000; i++) {
      longString += 'x';
    }

    // Act.
    lensForm.submitUrl(longString);
    await microtasksFinished();

    // Assert.
    assertFalse(urlFormSubmitted);
    assertEquals(LensErrorType.LENGTH_TOO_GREAT, lastError);
    assertEquals(null, lastSubmit);
  });

  test('submit url should set entrypoint parameter', async () => {
    // Arrange.
    const input =
        lensForm.$.urlForm.children.namedItem('ep') as HTMLInputElement;

    // Assert.
    assertEquals('cntpubu', input.value);
  });

  test('submit url should set rendering environment parameter', async () => {
    // Arrange.
    const input =
        lensForm.$.urlForm.children.namedItem('re') as HTMLInputElement;

    // Assert.
    assertEquals('df', input.value);
  });

  test('submit url should set surface parameter', async () => {
    // Arrange.
    const input =
        lensForm.$.urlForm.children.namedItem('s') as HTMLInputElement;

    // Assert.
    assertEquals('4', input.value);
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
    await microtasksFinished();

    // Assert.
    assertEquals('1001', input.value);
  });

  test(
      'submit url should set client data param to a non-empty value',
      async () => {
        // Arrange.
        const input =
            lensForm.$.urlForm.children.namedItem('cd') as HTMLInputElement;

        // Assert.
        assertGT(input.value.length, 0);
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
