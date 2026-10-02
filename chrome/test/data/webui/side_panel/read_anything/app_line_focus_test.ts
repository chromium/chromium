// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, ContentController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {ContentType, LineFocusController, LineFocusMovement, LineFocusStyle, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {microtasksFinished, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, setupAppTestEnvironment} from './common.js';
import type {TestVisualBrowserProxy} from './test_visual_browser_proxy.js';

suite('AppLineFocus', () => {
  let app: AppElement;
  let contentController: ContentController;
  let visualBrowserProxy: TestVisualBrowserProxy;

  function getLineFocusPadding(): number {
    const val = app.style.getPropertyValue('--line-focus-padding');
    return val ? parseInt(val) : 0;
  }

  setup(async () => {
    const result = await setupAppTestEnvironment();
    app = result.app;
    contentController = result.contentController;
    visualBrowserProxy = result.visualBrowserProxy;
  });

  test('connected callback adds line focus mouse listener', async () => {
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.CURSOR}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();
    let mouseMoveInToolbar = false;
    let mouseMove = false;
    LineFocusController.getInstance().onMouseMove = () => {
      mouseMove = true;
    };
    LineFocusController.getInstance().onMouseMoveInToolbar = () => {
      mouseMoveInToolbar = true;
    };

    app.$.containerParent.dispatchEvent(
        new MouseEvent('mousemove', {clientY: 10}));

    assertTrue(mouseMove);
    assertFalse(mouseMoveInToolbar);
  });

  test('new content updates padding for line focus', async () => {
    emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
        {detail: {data: LineFocusMovement.STATIC}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();
    assertEquals(0, getLineFocusPadding());

    app.updateContent();
    await whenCheck(app, () => getLineFocusPadding() !== 0);

    assertLT(0, getLineFocusPadding());
  });

  test(
      'new content does not update padding for line focus with flag disabled',
      async () => {
        visualBrowserProxy.lineFocusEnabled = false;
        app.connectedCallback();
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.CURSOR}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();
        assertEquals(0, getLineFocusPadding());

        app.updateContent();
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
      });

  test(
      'new content does not update padding for line focus with line focus off',
      async () => {
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.STATIC}});
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: false}});
        await microtasksFinished();
        assertEquals(0, getLineFocusPadding());

        app.updateContent();
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
      });

  test('line focus only shows on content', async () => {
    contentController.setState(ContentType.NO_CONTENT);
    await microtasksFinished();
    assertTrue(app.$.lineFocus.hasAttribute('hidden'));

    contentController.setState(ContentType.LOADING);
    await microtasksFinished();
    assertTrue(app.$.lineFocus.hasAttribute('hidden'));

    contentController.setState(ContentType.HAS_CONTENT);
    await microtasksFinished();
    assertFalse(app.$.lineFocus.hasAttribute('hidden'));
  });

  test(
      'onContentStateChange updates line focus style when enabled and ' +
          'has content',
      async () => {
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();

        contentController.setState(ContentType.HAS_CONTENT);
        await microtasksFinished();

        assertEquals(
            'block', app.style.getPropertyValue('--line-focus-display'));
      });

  test(
      'onContentStateChange disables line focus style when no content',
      async () => {
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();

        contentController.setState(ContentType.NO_CONTENT);
        await microtasksFinished();

        assertEquals(
            'none', app.style.getPropertyValue('--line-focus-display'));
      });

  test('onContentStateChange line focus showing if has content', async () => {
    emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();

    contentController.setState(ContentType.HAS_CONTENT);
    await microtasksFinished();

    assertTrue(app.$.toolbar.isLineFocusShowing);
  });

  test(
      'onContentStateChange line focus not showing if off but has content',
      async () => {
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: false}});
        await microtasksFinished();

        contentController.setState(ContentType.HAS_CONTENT);
        await microtasksFinished();

        assertFalse(app.$.toolbar.isLineFocusShowing);
      });

  test(
      'onContentStateChange line focus not showing if no content', async () => {
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        await microtasksFinished();

        contentController.setState(ContentType.NO_CONTENT);
        await microtasksFinished();

        assertFalse(app.$.toolbar.isLineFocusShowing);
      });

  test('showLoading marks line focus showing if enabled', async () => {
    emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
    emitEvent(
        app, ToolbarEvent.LINE_FOCUS_STYLE,
        {detail: {data: LineFocusStyle.UNDERLINE}});
    await microtasksFinished();

    app.showLoading();
    await microtasksFinished();

    assertTrue(app.$.toolbar.isLineFocusShowing);
  });

  test('showLoading does not mark line focus showing if disabled', async () => {
    emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: false}});
    await microtasksFinished();

    app.showLoading();
    await microtasksFinished();

    assertFalse(app.$.toolbar.isLineFocusShowing);
  });

  test('onNeedScrollForLineFocus scrolls', () => {
    const startingScrollTop = app.$.containerScroller.scrollTop;
    let scrollTo = 0;
    app.$.containerScroller.scrollTo = (options) => {
      scrollTo = (options as ScrollToOptions).top ?? 0;
    };

    const scrollDiff = 30;
    app.onNeedScrollForLineFocus(scrollDiff);

    assertEquals(startingScrollTop + scrollDiff, scrollTo);
  });
});
