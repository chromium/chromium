// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertNotStyle, assertStyle, keydown} from 'chrome://test/new_tab_page/test_support.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

/**
 * @param {!Element} element
 * @param {!Element} reference
 * @return {!{top: number, right: number, bottom: number, left: number}}
 */
function getRelativePosition(element, reference) {
  const referenceRect = reference.getBoundingClientRect();
  const elementRect = element.getBoundingClientRect();
  return {
    top: elementRect.top - referenceRect.top,
    right: elementRect.right - referenceRect.right,
    bottom: elementRect.bottom - referenceRect.bottom,
    left: elementRect.left - referenceRect.left,
  };
}

/**
 * @param {number} width
 * @param {number} height
 * @param {string} color
 * @return {string}
 */
function createImageDataUrl(width, height, color) {
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" ' +
      `width="${width}" height="${height}">` +
      `<rect width="100%" height="100%" fill="${color}"/>` +
      '</svg>';
  return `data:image/svg+xml;base64,${btoa(svg)}`;
}

/**
 * @param {number} width
 * @param {number} height
 * @return {!newTabPage.mojom.Doodle}
 */
function createImageDoodle(width, height) {
  width = width || 500;
  height = height || 200;
  return {
    image: {
      light: {
        imageUrl: {url: createImageDataUrl(width, height, 'red')},
        shareButton: {
          backgroundColor: {value: 0xddff0000},
          x: 10,
          y: 20,
          iconUrl: {url: 'data:bar'},
        },
        width,
        height,
        backgroundColor: {value: 0xffffffff},
        imageImpressionLogUrl: {url: 'https://log.com'},
      },
      dark: {
        imageUrl: {url: createImageDataUrl(width, height, 'blue')},
        shareButton: {
          backgroundColor: {value: 0xee00ff00},
          x: 30,
          y: 40,
          iconUrl: {url: 'data:foo'},
        },
        width,
        height,
        backgroundColor: {value: 0x000000ff},
        imageImpressionLogUrl: {url: 'https://dark_log.com'},
      },
      onClickUrl: {url: 'https://foo.com'},
    }
  };
}

suite('NewTabPageLogoTest', () => {
  /**
   * @implements {WindowProxy}
   * @extends {TestBrowserProxy}
   */
  let windowProxy;

  /**
   * @implements {newTabPage.mojom.PageHandlerRemote}
   * @extends {TestBrowserProxy}
   */
  let handler;

  async function createLogo(doodle = null) {
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: doodle,
    }));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);
    logo.backgroundColor = {value: 0xffffffff};
    await flushTasks();
    return logo;
  }

  setup(() => {
    PolymerTest.clearBody();

    windowProxy = TestBrowserProxy.fromClass(WindowProxy);
    windowProxy.setResultFor('createIframeSrc', '');
    handler = TestBrowserProxy.fromClass(newTabPage.mojom.PageHandlerRemote);
    handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: '',
      interactionLogUrl: null,
      shareId: '',
    }));
    WindowProxy.setInstance(windowProxy);
    NewTabPageProxy.setInstance(
        handler, new newTabPage.mojom.PageCallbackRouter());
  });

  [true, false].forEach(dark => {
    const darkStr = dark ? 'dark' : 'light';
    test(`setting ${darkStr} simple doodle shows image`, async () => {
      // Arrange.
      const doodle = createImageDoodle(/*width=*/ 500, /*height=*/ 200);
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;
      const shareButton = imageDoodle.shareButton;

      // Act.
      const logo = await createLogo(doodle);
      logo.dark = dark;
      logo.backgroundColor = imageDoodle.backgroundColor;

      // Assert.
      const el = selector => $$(logo, selector);
      assertNotStyle(el('#doodle'), 'display', 'none');
      assertFalse(!!el('#logo'));
      assertEquals(imageDoodle.imageUrl.url, el('#image').src);
      assertNotStyle(el('#image'), 'none');
      assertEquals(500, el('#image').offsetWidth);
      assertEquals(200, el('#image').offsetHeight);
      assertNotStyle(el('#shareButton'), 'display', 'none');
      assertStyle(
          el('#shareButton'), 'background-color',
          skColorToRgba(shareButton.backgroundColor));
      const buttonPos = getRelativePosition(el('#shareButton'), el('#image'));
      assertEquals(shareButton.x, buttonPos.left);
      assertEquals(shareButton.y, buttonPos.top);
      assertEquals(26, el('#shareButton').offsetWidth);
      assertEquals(26, el('#shareButton').offsetHeight);
      assertEquals(shareButton.iconUrl.url, el('#shareButtonImage').src);
      assertStyle(el('#animation'), 'display', 'none');
      assertFalse(!!el('#iframe'));
    });
  });

  [true, false].forEach(dark => {
    test(`hide share button in ${dark ? 'dark' : 'light'} mode`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;
      delete imageDoodle.shareButton;

      // Act.
      const logo = await createLogo(doodle);
      logo.dark = dark;

      // Assert.
      assertStyle($$(logo, '#shareButton'), 'display', 'none');
    });
  });

  [null, '#ff0000'].forEach(color => {
    test(`${color || 'no'} background color shows box`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      doodle.image.light.backgroundColor.value = 0xff0000ff;

      // Act.
      const logo = await createLogo(doodle);
      logo.backgroundColor = !color || hexColorToSkColor(color);

      // Assert.
      assertStyle($$(logo, '#imageDoodle'), 'padding', '16px 24px');
      assertStyle(
          $$(logo, '#imageDoodle'), 'background-color', 'rgb(0, 0, 255)');
    });
  });

  [[1000, 500] /* too large */,
   [100, 50] /* too small */,
  ].forEach(([width, height]) => {
    test(`${width}x${height} doodle aligned correctly`, async () => {
      // Act.
      const logo = await createLogo(createImageDoodle(width, height));
      logo.backgroundColor = {value: 0xffffffff};

      // Assert.
      assertEquals(200, logo.offsetHeight);
      assertGE(200, $$(logo, '#image').offsetHeight);
      const pos = getRelativePosition($$(logo, '#imageDoodle'), logo);
      assertLE(0, pos.top);
      assertEquals(0, pos.bottom);
    });

    test(`${width}x${height} boxed doodle aligned correctly`, async () => {
      // Act.
      const logo = await createLogo(createImageDoodle(width, height));
      logo.dark = true;
      logo.backgroundColor = {value: 0xff0000ff};

      // Assert.
      assertEquals(200, logo.offsetHeight);
      assertGE(160, $$(logo, '#image').offsetHeight);
      const pos = getRelativePosition($$(logo, '#imageDoodle'), logo);
      assertGE(pos.top, 8);
      assertEquals(0, pos.bottom);
    });
  });

  test('dark mode and no dark doodle shows logo', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    delete doodle.image.dark;

    // Act.
    const logo = await createLogo(doodle);
    logo.dark = true;

    // Assert.
    assertTrue(!!$$(logo, '#doodle'));
    assertFalse(!!$$(logo, '#logo'));
  });

  test('setting too large image doodle resizes image', async () => {
    // Arrange.
    const doodle = createImageDoodle(/*width=*/ 1000, /*height=*/ 500);
    doodle.image.light.shareButton.x = 10;
    doodle.image.light.shareButton.y = 20;

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertEquals(400, $$(logo, '#image').offsetWidth);
    assertEquals(200, $$(logo, '#image').offsetHeight);
    const shareButtonPosition =
        getRelativePosition($$(logo, '#shareButton'), $$(logo, '#image'));
    assertEquals(4, Math.round(shareButtonPosition.left));
    assertEquals(8, Math.round(shareButtonPosition.top));
    assertEquals(10, $$(logo, '#shareButton').offsetWidth);
    assertEquals(10, $$(logo, '#shareButton').offsetHeight);
  });

  test('setting animated doodle shows image', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image.light.imageUrl = {url: 'data:foo'};
    doodle.image.light.animationUrl = {url: 'https://foo.com'};

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#image').src, 'data:foo');
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertStyle($$(logo, '#animation'), 'display', 'none');
    assertFalse(!!$$(logo, '#iframe'));
  });

  test('setting interactive doodle shows iframe', async () => {
    // Act.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      }
    });
    logo.dark = false;

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertNotStyle($$(logo, '#iframe'), 'display', 'none');
    assertStyle($$(logo, '#iframe'), 'width', '200px');
    assertStyle($$(logo, '#iframe'), 'height', '100px');
    assertStyle($$(logo, '#imageDoodle'), 'display', 'none');
    assertEquals($$(logo, '#iframe').src, 'https://foo.com/?theme_messages=0');
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals($$($$(logo, '#iframe'), '#iframe'), iframe);
    assertEquals('changeMode', cmd);
    assertEquals(false, dark);
    assertEquals('https://foo.com', origin);
  });

  test('message only after mode has been set', async () => {
    // Act (no mode).
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      }
    });

    // Assert (no mode).
    assertEquals(0, windowProxy.getCallCount('postMessage'));

    // Act (setting mode).
    logo.dark = true;

    // Assert (setting mode).
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals($$($$(logo, '#iframe'), '#iframe'), iframe);
    assertEquals('changeMode', cmd);
    assertEquals(true, dark);
    assertEquals('https://foo.com', origin);
  });

  test('before doodle loaded shows nothing', () => {
    // Act.
    handler.setResultFor('getDoodle', new Promise(() => {}));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);

    // Assert.
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#doodle'), null);
  });

  test('unavailable doodle shows logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle($$(logo, '#logo'), 'display', 'none');
    assertEquals($$(logo, '#doodle'), null);
  });

  test('not setting-single colored shows multi-colored logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle($$(logo, '#logo'), 'background-image', '');
    assertStyle($$(logo, '#logo'), '-webkit-mask-image', 'none');
    assertStyle($$(logo, '#logo'), 'background-color', 'rgba(0, 0, 0, 0)');
  });

  test('setting single-colored shows single-colored logo', async () => {
    // Act.
    const logo = await createLogo();
    logo.singleColored = true;
    logo.style.setProperty('--ntp-logo-color', 'red');

    // Assert.
    assertNotStyle($$(logo, '#logo'), '-webkit-mask-image', 'none');
    assertStyle($$(logo, '#logo'), 'background-color', 'rgb(255, 0, 0)');
    assertStyle($$(logo, '#logo'), 'background-image', 'none');
  });

  test('logo aligned correctly', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    const pos = getRelativePosition($$(logo, '#logo'), logo);
    assertEquals(0, pos.bottom);
    assertEquals(92, $$(logo, '#logo').offsetHeight);
  });

  test('doodle aligned correctly', async () => {
    // Act.
    const logo = await createLogo(createImageDoodle());

    // Assert.
    const pos = getRelativePosition($$(logo, '#doodle'), logo);
    assertEquals(0, pos.bottom);
  });

  test('too large interactive doodle sized correctly', async () => {
    // Arrange.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 1000,
        height: 500,
      }
    });

    // Assert.
    assertEquals(200, logo.offsetHeight);
    assertEquals(200, $$(logo, '#iframe').offsetHeight);
    const pos = getRelativePosition($$(logo, '#doodle'), logo);
    assertEquals(0, pos.bottom);
  });

  test('receiving resize message resizes doodle', async () => {
    // Arrange.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      }
    });
    const transitionend = eventToPromise('transitionend', $$(logo, '#iframe'));

    // Act.
    window.postMessage(
        {
          cmd: 'resizeDoodle',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await transitionend;

    // Assert.
    const transitionedProperties = window.getComputedStyle($$(logo, '#iframe'))
                                       .getPropertyValue('transition-property')
                                       .trim()
                                       .split(',')
                                       .map(s => s.trim());
    assertStyle($$(logo, '#iframe'), 'transition-duration', '0.5s');
    assertTrue(transitionedProperties.includes('height'));
    assertTrue(transitionedProperties.includes('width'));
    assertEquals($$(logo, '#iframe').offsetHeight, 500);
    assertEquals($$(logo, '#iframe').offsetWidth, 700);
    assertGE(logo.offsetHeight, 500);
    assertGE(logo.offsetWidth, 700);
  });

  test('receiving other message does not resize doodle', async () => {
    // Arrange.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      }
    });
    const height = $$(logo, '#iframe').offsetHeight;
    const width = $$(logo, '#iframe').offsetWidth;

    // Act.
    window.postMessage(
        {
          cmd: 'foo',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await flushTasks();

    // Assert.
    assertEquals($$(logo, '#iframe').offsetHeight, height);
    assertEquals($$(logo, '#iframe').offsetWidth, width);
  });

  test('receiving mode message sends mode', async () => {
    // Arrange.
    const logo =
        await createLogo({interactive: {url: {url: 'https://foo.com'}}});
    logo.dark = false;
    windowProxy.resetResolver('postMessage');

    // Act.
    window.postMessage({cmd: 'sendMode'}, '*');
    await flushTasks();

    // Assert.
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [_, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals('changeMode', cmd);
    assertEquals(false, dark);
    assertEquals('https://foo.com', origin);
  });

  test('clicking simple doodle opens link', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image.onClickUrl = {url: 'https://foo.com'};
    const logo = await createLogo(doodle);

    // Act.
    $$(logo, '#image').click();
    const url = await windowProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://foo.com/');
  });

  [' ', 'Enter'].forEach(key => {
    test(`pressing ${key} on simple doodle opens link`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      doodle.image.onClickUrl = {url: 'https://foo.com'};
      const logo = await createLogo(doodle);

      // Act.
      keydown($$(logo, '#image'), key);
      const url = await windowProxy.whenCalled('open');

      // Assert.
      assertEquals(url, 'https://foo.com/');
    });
  });

  test('animated doodle starts and stops', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image.light.animationUrl = {url: 'https://foo.com'};
    const logo = await createLogo(doodle);

    // Act (start animation).
    $$(logo, '#image').click();

    // Assert (animation started).
    assertEquals(windowProxy.getCallCount('open'), 0);
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertNotStyle($$(logo, '#animation'), 'display', 'none');
    assertEquals(
        $$(logo, '#animation').src,
        'chrome-untrusted://new-tab-page/image?https://foo.com');
    assertDeepEquals(
        $$(logo, '#image').getBoundingClientRect(),
        $$(logo, '#animation').getBoundingClientRect());

    // Act (switch mode).
    logo.dark = true;

    // Assert (animation stopped).
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertStyle($$(logo, '#animation'), 'display', 'none');
  });

  test('clicking animation of animated doodle opens link', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image.light.animationUrl = {url: 'https://foo.com'};
    doodle.image.onClickUrl = {url: 'https://bar.com'};
    const logo = await createLogo(doodle);
    $$(logo, '#image').click();

    // Act.
    $$(logo, '#animation').click();
    const url = await windowProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://bar.com/');
  });

  test('share dialog removed on start', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());

    // Assert.
    assertFalse(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  test('clicking share button adds share dialog', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());

    // Act.
    $$(logo, '#shareButton').click();
    await flushTasks();

    // Assert.
    assertTrue(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  test('closing share dialog removes share dialog', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());
    $$(logo, '#shareButton').click();
    await flushTasks();

    // Act.
    logo.shadowRoot.querySelector('ntp-doodle-share-dialog')
        .dispatchEvent(new Event('close'));
    await flushTasks();

    // Assert.
    assertFalse(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  [true, false].forEach(dark => {
    const darkStr = dark ? 'dark' : 'light';
    test(`${darkStr} simple doodle logging flow`, async () => {
      // Arrange.
      const doodleResolver = new PromiseResolver();
      handler.setResultFor('getDoodle', doodleResolver.promise);
      const logo = document.createElement('ntp-logo');
      document.body.appendChild(logo);
      logo.dark = dark;
      handler.setResultFor('onDoodleImageRendered', Promise.resolve({
        imageClickParams: 'foo=bar&hello=world',
        interactionLogUrl: null,
        shareId: '123',
      }));
      const doodle = createImageDoodle();
      doodle.image.onClickUrl = {url: 'https://click.com?ct=supi'};
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;

      // Act (load).
      doodleResolver.resolve({doodle});
      await flushTasks();

      // Assert (load).
      const [type, _, logUrl] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(newTabPage.mojom.DoodleImageType.kStatic, type);
      assertEquals(imageDoodle.imageImpressionLogUrl.url, logUrl.url);

      // Act (click).
      $$(logo, '#image').click();

      // Assert (click).
      const [type2] = await handler.whenCalled('onDoodleImageClicked');
      const onClickUrl = await windowProxy.whenCalled('open');
      assertEquals(newTabPage.mojom.DoodleImageType.kStatic, type2);
      assertEquals(
          'https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

      // Act (share).
      $$(logo, '#shareButton').click();
      await flushTasks();
      $$(logo, 'ntp-doodle-share-dialog')
          .dispatchEvent(new CustomEvent(
              'share',
              {detail: newTabPage.mojom.DoodleShareChannel.kFacebook}));

      // Assert (share).
      const [channel, doodleId, shareId] =
          await handler.whenCalled('onDoodleShared');
      assertEquals(newTabPage.mojom.DoodleShareChannel.kFacebook, channel);
      assertEquals('supi', doodleId);
      assertEquals('123', shareId);
    });

    test(`${darkStr} animated doodle logging flow`, async () => {
      // Arrange.
      const doodleResolver = new PromiseResolver();
      handler.setResultFor('getDoodle', doodleResolver.promise);
      const logo = document.createElement('ntp-logo');
      document.body.appendChild(logo);
      logo.dark = dark;
      handler.setResultFor('onDoodleImageRendered', Promise.resolve({
        imageClickParams: '',
        interactionLogUrl: {url: 'https://interaction.com'},
        shareId: '',
      }));
      const doodle = createImageDoodle();
      doodle.image.onClickUrl = {url: 'https://click.com?ct=supi'};
      doodle.image.light.animationUrl = {url: 'https://animation.com'};
      doodle.image.dark.animationUrl = {url: 'https://dark_animation.com'};
      doodle.image.light.animationImpressionLogUrl = {
        url: 'https://animation_log.com'
      };
      doodle.image.dark.animationImpressionLogUrl = {
        url: 'https://dark_animation_log.com'
      };
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;

      // Act (CTA load).
      doodleResolver.resolve({doodle});
      await flushTasks();

      // Assert (CTA load).
      const [type, _, logUrl] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(newTabPage.mojom.DoodleImageType.kCta, type);
      assertEquals(imageDoodle.imageImpressionLogUrl.url, logUrl.url);

      // Act (CTA click).
      handler.resetResolver('onDoodleImageRendered');
      handler.setResultFor('onDoodleImageRendered', Promise.resolve({
        imageClickParams: 'foo=bar&hello=world',
        interactionLogUrl: null,
        shareId: '123',
      }));
      $$(logo, '#image').click();

      // Assert (CTA click).
      const [type2, interactionLogUrl] =
          await handler.whenCalled('onDoodleImageClicked');
      assertEquals(newTabPage.mojom.DoodleImageType.kCta, type2);
      assertEquals('https://interaction.com', interactionLogUrl.url);

      // Assert (animation load). Also triggered by clicking #image.
      const [type3, __, logUrl2] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(newTabPage.mojom.DoodleImageType.kAnimation, type3);
      assertEquals(imageDoodle.animationImpressionLogUrl.url, logUrl2.url);

      // Act (animation click).
      handler.resetResolver('onDoodleImageClicked');
      $$(logo, '#animation').click();

      // Assert (animation click).
      const [type4, ___] = await handler.whenCalled('onDoodleImageClicked');
      const onClickUrl = await windowProxy.whenCalled('open');
      assertEquals(newTabPage.mojom.DoodleImageType.kAnimation, type4);
      assertEquals(
          'https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

      // Act (share).
      $$(logo, '#shareButton').click();
      await flushTasks();
      $$(logo, 'ntp-doodle-share-dialog')
          .dispatchEvent(new CustomEvent(
              'share', {detail: newTabPage.mojom.DoodleShareChannel.kTwitter}));

      // Assert (share).
      const [channel, doodleId, shareId] =
          await handler.whenCalled('onDoodleShared');
      assertEquals(newTabPage.mojom.DoodleShareChannel.kTwitter, channel);
      assertEquals('supi', doodleId);
      assertEquals('123', shareId);
    });
  });
});
