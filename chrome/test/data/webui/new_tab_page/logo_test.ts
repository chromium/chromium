// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {IframeElement, LogoElement} from 'chrome://new-tab-page/new_tab_page.js';
import {$$, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {Doodle} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {DoodleImageType, DoodleShareChannel, PageCallbackRouter, PageHandlerRemote} from 'chrome://new-tab-page/new_tab_page.mojom-webui.js';
import {hexColorToSkColor} from 'chrome://resources/js/color_utils.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertLE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertNotStyle, assertStyle, installMock, keydown} from './test_support.js';

/**
 * @return {!{top: number, right: number, bottom: number, left: number}}
 */
function getRelativePosition(element: Element, reference: Element) {
  const referenceRect = reference.getBoundingClientRect();
  const elementRect = element.getBoundingClientRect();
  return {
    top: elementRect.top - referenceRect.top,
    right: elementRect.right - referenceRect.right,
    bottom: elementRect.bottom - referenceRect.bottom,
    left: elementRect.left - referenceRect.left,
  };
}

function createImageDataUrl(
    width: number, height: number, color: string): string {
  const svg = '<svg xmlns="http://www.w3.org/2000/svg" ' +
      `width="${width}" height="${height}">` +
      `<rect width="100%" height="100%" fill="${color}"/>` +
      '</svg>';
  return `data:image/svg+xml;base64,${btoa(svg)}`;
}

function createImageDoodle(width: number = 500, height: number = 200): Doodle {
  return {
    image: {
      light: {
        animationUrl: null,
        animationImpressionLogUrl: null,
        imageUrl: {url: createImageDataUrl(width, height, 'red')},
        width,
        height,
        backgroundColor: {value: 0xffffffff},
        imageImpressionLogUrl: {url: 'https://log.com'},
      },
      dark: {
        animationUrl: null,
        animationImpressionLogUrl: null,
        imageUrl: {url: createImageDataUrl(width, height, 'blue')},
        width,
        height,
        backgroundColor: {value: 0x000000ff},
        imageImpressionLogUrl: {url: 'https://dark_log.com'},
      },
      onClickUrl: {url: 'https://foo.com'},
      shareUrl: {url: 'https://foo.com'},
    },
    description: 'Dummy',
    interactive: null,
  };
}

suite('NewTabPageLogoTest', () => {
  let windowProxy: TestMock<WindowProxy>;
  let handler: TestMock<PageHandlerRemote>;

  async function createLogo(doodle: Doodle|null = null): Promise<LogoElement> {
    handler.setResultFor('getDoodle', Promise.resolve({
      doodle: doodle,
    }));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);
    logo.backgroundColor = {value: 0xffffffff};
    await microtasksFinished();
    return logo;
  }

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    windowProxy = installMock(WindowProxy);
    windowProxy.setResultFor('createIframeSrc', '');
    handler = installMock(
        PageHandlerRemote,
        mock => NewTabPageProxy.setInstance(mock, new PageCallbackRouter()));
    handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: '',
      interactionLogUrl: null,
      shareId: '',
    }));
  });

  [true, false].forEach(dark => {
    const darkStr = dark ? 'dark' : 'light';
    test(`setting ${darkStr} simple doodle shows image`, async () => {
      // Arrange.
      const doodle = createImageDoodle(/*width=*/ 500, /*height=*/ 168);
      assertTrue(!!doodle.image);
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;
      assertTrue(!!imageDoodle);

      // Act.
      const logo = await createLogo(doodle);
      logo.dark = dark;
      logo.backgroundColor = imageDoodle.backgroundColor;
      await microtasksFinished();

      // Assert.
      assertNotStyle($$(logo, '#doodle')!, 'display', 'none');
      assertFalse(!!$$(logo, '#logo'));
      assertEquals(
          imageDoodle.imageUrl.url, $$<HTMLImageElement>(logo, '#image')!.src);
      assertNotStyle($$(logo, '#image')!, 'display', 'none');
      assertEquals(500, $$<HTMLElement>(logo, '#image')!.offsetWidth);
      assertEquals(168, $$<HTMLElement>(logo, '#image')!.offsetHeight);
      assertNotStyle($$(logo, '#shareButton')!, 'display', 'none');
      assertEquals(32, $$<HTMLElement>(logo, '#shareButton')!.offsetWidth);
      assertEquals(32, $$<HTMLElement>(logo, '#shareButton')!.offsetHeight);
      assertStyle($$(logo, '#animation')!, 'display', 'none');
      assertFalse(!!$$(logo, '#iframe'));
    });
  });

  [null, '#ff0000'].forEach(color => {
    test(`${color || 'no'} background color shows box`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      assertTrue(!!doodle.image);
      doodle.image.light.backgroundColor.value = 0xff0000ff;

      // Act.
      const logo = await createLogo(doodle);
      if (color) {
        logo.backgroundColor = hexColorToSkColor(color);
      }

      // Assert.
      assertStyle($$(logo, '#imageDoodle')!, 'padding', '16px 24px');
      assertStyle(
          $$(logo, '#imageDoodle')!, 'background-color', 'rgb(0, 0, 255)');
    });
  });

  [true, false].forEach(short => {
    suite(`${short ? 'short' : 'regular'} height`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({reducedLogoSpaceEnabled: short});
      });

      [[1000, 500] /* too large */,
       [100, 50] /* too small */,
      ].forEach(([width, height]) => {
        test(`${width}x${height} doodle aligned correctly`, async () => {
          // Act.
          const logo = await createLogo(createImageDoodle(width, height));
          logo.backgroundColor = {value: 0xffffffff};

          // Assert.
          assertEquals(short ? 168 : 200, logo.offsetHeight);
          assertGE(200, $$<HTMLElement>(logo, '#image')!.offsetHeight);
          const pos = getRelativePosition($$(logo, '#imageDoodle')!, logo);
          assertLE(0, pos.top);
          assertEquals(0, pos.bottom);
        });

        test(`${width}x${height} boxed doodle aligned correctly`, async () => {
          // Act.
          const logo = await createLogo(createImageDoodle(width, height));
          logo.dark = true;
          logo.backgroundColor = {value: 0xff0000ff};
          await microtasksFinished();

          // Assert.
          assertEquals(short ? 168 : 200, logo.offsetHeight);
          assertGE(160, $$<HTMLElement>(logo, '#image')!.offsetHeight);
          const pos = getRelativePosition($$(logo, '#imageDoodle')!, logo);
          assertGE(pos.top, 8);
          assertEquals(0, pos.bottom);
        });
      });
    });
  });

  test('dark mode and no dark doodle shows logo', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image!.dark = null;

    // Act.
    const logo = await createLogo(doodle);
    logo.dark = true;
    await microtasksFinished();

    // Assert.
    assertFalse(!!$$(logo, '#doodle'));
    assertTrue(!!$$(logo, '#logo'));
  });

  [true, false].forEach(short => {
    suite(`${short ? 'short' : 'regular'} height`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({reducedLogoSpaceEnabled: short});
      });

      test('setting too large image doodle resizes image', async () => {
        // Arrange.
        const doodle = createImageDoodle(/*width=*/ 1000, /*height=*/ 500);

        // Act.
        const logo = await createLogo(doodle);

        // Assert.
        assertEquals(
            short ? 336 : 400, $$<HTMLElement>(logo, '#image')!.offsetWidth);
        assertEquals(
            short ? 168 : 200, $$<HTMLElement>(logo, '#image')!.offsetHeight);
        assertEquals(32, $$<HTMLElement>(logo, '#shareButton')!.offsetWidth);
        assertEquals(32, $$<HTMLElement>(logo, '#shareButton')!.offsetHeight);
      });
    });
  });

  test('setting animated doodle shows image', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.image!.light!.imageUrl = {url: 'data:foo'};
    doodle.image!.light!.animationUrl = {url: 'https://foo.com'};

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertNotStyle($$(logo, '#doodle')!, 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$<IframeElement>(logo, '#image')!.src, 'data:foo');
    assertNotStyle($$(logo, '#image')!, 'display', 'none');
    assertStyle($$(logo, '#animation')!, 'display', 'none');
    assertFalse(!!$$(logo, '#iframe'));
  });

  test('setting interactive doodle shows iframe', async () => {
    // Act.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      },
      description: '',
      image: null,
    });
    logo.dark = false;
    await microtasksFinished();

    // Assert.
    assertNotStyle($$(logo, '#doodle')!, 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertNotStyle($$(logo, '#iframe')!, 'display', 'none');
    assertStyle($$(logo, '#iframe')!, 'width', '200px');
    assertStyle($$(logo, '#iframe')!, 'height', '100px');
    assertStyle($$(logo, '#imageDoodle')!, 'display', 'none');
    assertEquals(
        $$<IframeElement>(logo, '#iframe')!.src,
        'https://foo.com/?theme_messages=0');
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals($$($$(logo, '#iframe')!, '#iframe'), iframe);
    assertEquals('changeMode', cmd);
    assertFalse(dark);
    assertEquals('https://foo.com', origin);
  });

  test('message only after mode has been set', async () => {
    // Act (no mode).
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      },
      description: '',
      image: null,
    });

    // Assert (no mode).
    assertEquals(0, windowProxy.getCallCount('postMessage'));

    // Act (setting mode).
    logo.dark = true;
    await microtasksFinished();

    // Assert (setting mode).
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [iframe, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals($$($$(logo, '#iframe')!, '#iframe'), iframe);
    assertEquals('changeMode', cmd);
    assertTrue(dark);
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
    assertNotStyle($$(logo, '#logo')!, 'display', 'none');
    assertEquals($$(logo, '#doodle'), null);
  });

  test('not setting-single colored shows multi-colored logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle($$(logo, '#logo')!, 'background-image', '');
    assertStyle($$(logo, '#logo')!, '-webkit-mask-image', 'none');
    assertStyle($$(logo, '#logo')!, 'background-color', 'rgba(0, 0, 0, 0)');
  });

  test('setting single-colored shows single-colored logo', async () => {
    // Act.
    const logo = await createLogo();
    logo.singleColored = true;
    logo.style.setProperty('--ntp-logo-color', 'red');
    await microtasksFinished();

    // Assert.
    assertNotStyle($$(logo, '#logo')!, '-webkit-mask-image', 'none');
    assertStyle($$(logo, '#logo')!, 'background-color', 'rgb(255, 0, 0)');
    assertStyle($$(logo, '#logo')!, 'background-image', 'none');
  });

  test('logo aligned correctly', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    const pos = getRelativePosition($$(logo, '#logo')!, logo);
    assertEquals(0, pos.bottom);
    assertEquals(92, $$<HTMLElement>(logo, '#logo')!.offsetHeight);
  });

  test('doodle aligned correctly', async () => {
    // Act.
    const logo = await createLogo(createImageDoodle());

    // Assert.
    const pos = getRelativePosition($$(logo, '#doodle')!, logo);
    assertEquals(0, pos.bottom);
  });

  [true, false].forEach(short => {
    suite(`${short ? 'short' : 'regular'} height`, () => {
      suiteSetup(() => {
        loadTimeData.overrideValues({reducedLogoSpaceEnabled: short});
      });

      test('too large interactive doodle sized correctly', async () => {
        // Arrange.
        const logo = await createLogo({
          interactive: {
            url: {url: 'https://foo.com'},
            width: 1000,
            height: 500,
          },
          description: '',
          image: null,
        });

        // Assert.
        assertEquals(short ? 168 : 200, logo.offsetHeight);
        assertEquals(
            short ? 168 : 200, $$<HTMLElement>(logo, '#iframe')!.offsetHeight);
        const pos = getRelativePosition($$(logo, '#doodle')!, logo);
        assertEquals(0, pos.bottom);
      });
    });
  });

  test('receiving resize message resizes doodle', async () => {
    // Arrange.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      },
      description: '',
      image: null,
    });

    // Wait for one frame, to ensure the transition starts after the iframe has
    // been rendered.
    await new Promise<void>(resolve => requestAnimationFrame(() => resolve()));

    const transitionend = eventToPromise('transitionend', $$(logo, '#iframe')!);
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
    const transitionedProperties = window.getComputedStyle($$(logo, '#iframe')!)
                                       .getPropertyValue('transition-property')
                                       .trim()
                                       .split(',')
                                       .map(s => s.trim());
    assertStyle($$(logo, '#iframe')!, 'transition-duration', '0.5s');
    assertTrue(transitionedProperties.includes('height'));
    assertTrue(transitionedProperties.includes('width'));
    assertEquals($$<HTMLElement>(logo, '#iframe')!.offsetHeight, 500);
    assertEquals($$<HTMLElement>(logo, '#iframe')!.offsetWidth, 700);
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
      },
      description: '',
      image: null,
    });
    const height = $$<HTMLElement>(logo, '#iframe')!.offsetHeight;
    const width = $$<HTMLElement>(logo, '#iframe')!.offsetWidth;

    // Act.
    window.postMessage(
        {
          cmd: 'foo',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await microtasksFinished();

    // Assert.
    assertEquals($$<HTMLElement>(logo, '#iframe')!.offsetHeight, height);
    assertEquals($$<HTMLElement>(logo, '#iframe')!.offsetWidth, width);
  });

  test('receiving mode message sends mode', async () => {
    // Arrange.
    const logo = await createLogo({
      interactive: {
        url: {url: 'https://foo.com'},
        width: 200,
        height: 100,
      },
      description: '',
      image: null,
    });
    logo.dark = false;
    await microtasksFinished();
    windowProxy.resetResolver('postMessage');

    // Act.
    window.postMessage({cmd: 'sendMode'}, '*');
    await microtasksFinished();

    // Assert.
    assertEquals(1, windowProxy.getCallCount('postMessage'));
    const [_, {cmd, dark}, origin] =
        await windowProxy.whenCalled('postMessage');
    assertEquals('changeMode', cmd);
    assertEquals(false, dark);
    assertEquals('https://foo.com', origin);
  });

  [true, false].forEach(hasUrl => {
    const withOut = hasUrl ? 'with' : 'without';
    test(`clicking simple doodle ${withOut} URL`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      doodle.image!.onClickUrl = hasUrl ? {url: 'https://foo.com'} : null;
      const logo = await createLogo(doodle);

      // Act.
      $$<HTMLElement>(logo, '#image')!.click();

      // Assert.
      assertEquals(hasUrl ? 1 : 0, windowProxy.getCallCount('open'));
      if (hasUrl) {
        assertEquals('https://foo.com/', windowProxy.getArgs('open')[0]);
      }
      assertEquals(
          hasUrl ? 0 : -1, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);
    });

    [' ', 'Enter'].forEach(key => {
      test(`pressing ${key} on simple doodle ${withOut} URL`, async () => {
        // Arrange.
        const doodle = createImageDoodle();
        doodle.image!.onClickUrl = hasUrl ? {url: 'https://foo.com'} : null;
        const logo = await createLogo(doodle);

        // Act.
        keydown($$<HTMLElement>(logo, '#image')!, key);

        // Assert.
        assertEquals(hasUrl ? 1 : 0, windowProxy.getCallCount('open'));
        if (hasUrl) {
          assertEquals('https://foo.com/', windowProxy.getArgs('open')[0]);
        }
        assertEquals(
            hasUrl ? 0 : -1, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);
      });
    });

    test(`animated doodle starts and stops ${withOut} URL`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      assertTrue(!!doodle.image);
      doodle.image.light.animationUrl = {url: 'https://foo.com'};
      doodle.image.onClickUrl = hasUrl ? {url: 'https://bar.com'} : null;
      const logo = await createLogo(doodle);
      assertEquals(0, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);

      // Act (start animation).
      $$<HTMLElement>(logo, '#image')!.click();
      await microtasksFinished();

      // Assert (animation started).
      assertEquals(windowProxy.getCallCount('open'), 0);
      assertNotStyle($$(logo, '#image')!, 'display', 'none');
      assertNotStyle($$(logo, '#animation')!, 'display', 'none');
      assertEquals(
          $$<IframeElement>(logo, '#animation')!.src,
          'chrome-untrusted://new-tab-page/image?https://foo.com');
      assertDeepEquals(
          $$(logo, '#image')!.getBoundingClientRect(),
          $$(logo, '#animation')!.getBoundingClientRect());
      assertEquals(
          hasUrl ? 0 : -1, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);

      // Act (switch mode).
      logo.dark = true;
      await microtasksFinished();

      // Assert (animation stopped).
      assertNotStyle($$(logo, '#image')!, 'display', 'none');
      assertStyle($$(logo, '#animation')!, 'display', 'none');
      assertEquals(
          hasUrl ? 0 : -1, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);
    });

    test(`clicking animation of animated doodle ${withOut} URL`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      assertTrue(!!doodle.image);
      assertTrue(!!doodle.image.light);
      doodle.image.light.animationUrl = {url: 'https://foo.com'};
      doodle.image.onClickUrl = hasUrl ? {url: 'https://bar.com'} : null;
      const logo = await createLogo(doodle);
      $$<HTMLElement>(logo, '#image')!.click();
      await microtasksFinished();

      // Act.
      $$<HTMLElement>(logo, '#animation')!.click();

      // Assert.
      assertEquals(hasUrl ? 1 : 0, windowProxy.getCallCount('open'));
      if (hasUrl) {
        assertEquals('https://bar.com/', windowProxy.getArgs('open')[0]);
      }
      assertEquals(
          hasUrl ? 0 : -1, $$<HTMLElement>(logo, '#imageDoodle')!.tabIndex);
    });
  });

  test('share dialog removed on start', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());

    // Assert.
    assertFalse(!!logo.shadowRoot!.querySelector('ntp-doodle-share-dialog'));
  });

  test('clicking share button adds share dialog', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    const logo = await createLogo(doodle);

    // Act.
    $$<HTMLElement>(logo, '#shareButton')!.click();
    await microtasksFinished();

    // Assert.
    const dialog = logo.shadowRoot!.querySelector('ntp-doodle-share-dialog');
    assertTrue(!!dialog);
    assertEquals(doodle.description, dialog.title);
    // Check that the title is not set as an HTML attribute, which would result
    // in showing a tooltip on hover.
    assertFalse(dialog.hasAttribute('title'));
  });

  test('closing share dialog removes share dialog', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());
    $$<HTMLElement>(logo, '#shareButton')!.click();
    await microtasksFinished();

    // Act.
    logo.shadowRoot!.querySelector('ntp-doodle-share-dialog')!.dispatchEvent(
        new Event('close'));
    await microtasksFinished();

    // Assert.
    assertFalse(!!logo.shadowRoot!.querySelector('ntp-doodle-share-dialog'));
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
      assertTrue(!!doodle.image);
      doodle.image.onClickUrl = {url: 'https://click.com?ct=supi'};
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;
      assertTrue(!!imageDoodle);

      // Act (load).
      doodleResolver.resolve({doodle});
      await microtasksFinished();

      // Assert (load).
      const [type, _, logUrl] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(DoodleImageType.kStatic, type);
      assertEquals(imageDoodle.imageImpressionLogUrl.url, logUrl.url);

      // Act (click).
      $$<HTMLElement>(logo, '#image')!.click();

      // Assert (click).
      const [type2] = await handler.whenCalled('onDoodleImageClicked');
      const onClickUrl = await windowProxy.whenCalled('open');
      assertEquals(DoodleImageType.kStatic, type2);
      assertEquals(
          'https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

      // Act (share).
      $$<HTMLElement>(logo, '#shareButton')!.click();
      await microtasksFinished();
      ($$(logo, 'ntp-doodle-share-dialog')!
       ).dispatchEvent(new CustomEvent('share', {
        detail: DoodleShareChannel.kFacebook,
      }));

      // Assert (share).
      const [channel, doodleId, shareId] =
          await handler.whenCalled('onDoodleShared');
      assertEquals(DoodleShareChannel.kFacebook, channel);
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
      assertTrue(!!doodle.image);
      assertTrue(!!doodle.image.dark);
      assertTrue(!!doodle.image.light);
      doodle.image.onClickUrl = {url: 'https://click.com?ct=supi'};
      doodle.image.light.animationUrl = {url: 'https://animation.com'};
      doodle.image.dark.animationUrl = {url: 'https://dark_animation.com'};
      doodle.image.light.animationImpressionLogUrl = {
        url: 'https://animation_log.com',
      };
      doodle.image.dark.animationImpressionLogUrl = {
        url: 'https://dark_animation_log.com',
      };
      const imageDoodle = dark ? doodle.image.dark : doodle.image.light;

      // Act (CTA load).
      doodleResolver.resolve({doodle});
      await microtasksFinished();

      // Assert (CTA load).
      const [type, _, logUrl] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(DoodleImageType.kCta, type);
      assertEquals(imageDoodle.imageImpressionLogUrl.url, logUrl.url);

      // Act (CTA click).
      handler.resetResolver('onDoodleImageRendered');
      handler.setResultFor('onDoodleImageRendered', Promise.resolve({
        imageClickParams: 'foo=bar&hello=world',
        interactionLogUrl: null,
        shareId: '123',
      }));
      $$<HTMLElement>(logo, '#image')!.click();

      // Assert (CTA click).
      const [type2, interactionLogUrl] =
          await handler.whenCalled('onDoodleImageClicked');
      assertEquals(DoodleImageType.kCta, type2);
      assertEquals('https://interaction.com', interactionLogUrl.url);

      // Assert (animation load). Also triggered by clicking #image.
      const [type3, __, logUrl2] =
          await handler.whenCalled('onDoodleImageRendered');
      assertEquals(DoodleImageType.kAnimation, type3);
      assertEquals(imageDoodle.animationImpressionLogUrl!.url, logUrl2.url);

      // Act (animation click).
      handler.resetResolver('onDoodleImageClicked');
      $$<HTMLElement>(logo, '#animation')!.click();

      // Assert (animation click).
      const [type4, ___] = await handler.whenCalled('onDoodleImageClicked');
      const onClickUrl = await windowProxy.whenCalled('open');
      assertEquals(DoodleImageType.kAnimation, type4);
      assertEquals(
          'https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

      // Act (share).
      $$<HTMLElement>(logo, '#shareButton')!.click();
      await microtasksFinished();
      ($$(logo, 'ntp-doodle-share-dialog')!
       ).dispatchEvent(new CustomEvent('share', {
        detail: DoodleShareChannel.kTwitter,
      }));

      // Assert (share).
      const [channel, doodleId, shareId] =
          await handler.whenCalled('onDoodleShared');
      assertEquals(DoodleShareChannel.kTwitter, channel);
      assertEquals('supi', doodleId);
      assertEquals('123', shareId);
    });
  });
});
