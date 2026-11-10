// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';

import type {AppElement, ContentController, ContentPosition, SelectionController, SpeechController} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {calculateTextBounds, ContentPositionSource, ContentType, LineFocusController, LineFocusMovement, LineFocusStyle, ToolbarEvent} from 'chrome-untrusted://read-anything-side-panel.top-chrome/read_anything.js';
import {assertEquals, assertFalse, assertLT, assertTrue} from 'chrome-untrusted://webui-test/chai_assert.js';
import {eventToPromise, microtasksFinished, whenCheck} from 'chrome-untrusted://webui-test/test_util.js';

import {emitEvent, setupAppTestEnvironment} from './common.js';

suite('AppLineFocus', () => {
  let app: AppElement;
  let contentController: ContentController;
  let selectionController: SelectionController;
  let speechController: SpeechController;

  function getLineFocusPadding(): number {
    const val = app.style.getPropertyValue('--line-focus-padding');
    return val ? parseInt(val) : 0;
  }

  setup(async () => {
    const result = await setupAppTestEnvironment();
    app = result.app;
    contentController = result.contentController;
    selectionController = result.selectionController;
    speechController = result.speechController;
  });

  teardown(() => {
    document.body.style.height = '';
    app.style.height = '';
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

  test('container scroll notifies line focus of scroll', () => {
    let scrolled = false;
    let textLocationsChanged = false;
    LineFocusController.getInstance().onScroll = () => {
      scrolled = true;
    };
    LineFocusController.getInstance().onTextLocationsChange = () => {
      textLocationsChanged = true;
    };

    app.$.containerScroller.dispatchEvent(new Event('scroll'));

    assertTrue(scrolled);
    assertFalse(textLocationsChanged);
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

  test(
      'switching from static to cursor at top keeps first line focused',
      async () => {
        document.body.style.height = '400px';
        app.style.height = '400px';
        const paragraph = document.createElement('p');
        paragraph.style.whiteSpace = 'pre';
        paragraph.style.margin = '0';
        paragraph.style.fontSize = '20px';
        paragraph.style.lineHeight = '30px';
        paragraph.textContent =
            Array.from({length: 30}, (_, i) => `Line ${i}`).join('\n');
        app.$.container.replaceChildren(paragraph);
        contentController.setState(ContentType.HAS_CONTENT);
        await microtasksFinished();

        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.SMALL_WINDOW}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.STATIC}});
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
        await microtasksFinished();
        if (app.$.containerScroller.scrollTop !== 20) {
          const setupScrollEnd =
              eventToPromise('scrollend', app.$.containerScroller);
          app.$.containerScroller.scrollTop = 20;
          await setupScrollEnd;
        }

        assertLT(0, getLineFocusPadding());
        const staticY =
            Number.parseFloat(app.style.getPropertyValue('--line-focus-y'));

        const cursorScrollEnd =
            eventToPromise('scrollend', app.$.containerScroller);
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.CURSOR}});
        // The mouse is over the toolbar when switching modes from the menu.
        app.$.toolbar.dispatchEvent(
            new MouseEvent('mousemove', {clientY: 250}));
        await cursorScrollEnd;
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
        const cursorY =
            Number.parseFloat(app.style.getPropertyValue('--line-focus-y'));
        const firstLineTop =
            calculateTextBounds(
                app.$.container, app.$.appFlexParent.clientHeight)
                .bounds[0]!.top;
        assertLT(cursorY, staticY);
        assertEquals(firstLineTop, cursorY);
      });

  suite('deferred hit testing', () => {
    let originalCaretPositionFromPoint: typeof document.caretPositionFromPoint;
    let caretCalls: number;
    let caretY: number|null;
    let caretPosition: CaretPosition|null;

    setup(async () => {
      originalCaretPositionFromPoint = document.caretPositionFromPoint;
      caretCalls = 0;
      caretY = null;
      caretPosition = null;
      document.caretPositionFromPoint = (x, y, options) => {
        caretCalls++;
        caretY = y;
        caretPosition =
            originalCaretPositionFromPoint.call(document, x, y, options);
        return caretPosition;
      };

      emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
      emitEvent(
          app, ToolbarEvent.LINE_FOCUS_STYLE,
          {detail: {data: LineFocusStyle.UNDERLINE}});
      await microtasksFinished();
    });

    teardown(() => {
      document.caretPositionFromPoint = originalCaretPositionFromPoint;
    });

    test(
        'mouse move updates styles but defers hit testing until play',
        async () => {
          let onLineFocusChangeCalls = 0;
          let onLineFocusPosition: CaretPosition|null = null;
          const originalOnLineFocusChange =
              speechController.onLineFocusChange.bind(speechController);
          speechController.onLineFocusChange =
              (position: CaretPosition|null) => {
                onLineFocusChangeCalls++;
                onLineFocusPosition = position;
                originalOnLineFocusChange(position);
              };

          app.onLineFocusContentPositionChange(10, 20, 50);

          assertEquals('10px', app.style.getPropertyValue('--line-focus-y'));
          assertEquals(
              '20px', app.style.getPropertyValue('--line-focus-height'));
          assertEquals(0, caretCalls);
          assertEquals(0, onLineFocusChangeCalls);

          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();

          assertEquals(1, caretCalls);
          assertEquals(50, caretY);
          assertEquals(1, onLineFocusChangeCalls);
          assertEquals(caretPosition, onLineFocusPosition);

          // Subsequent play toggles do not re-query caret position since the
          // pending focal point was already consumed.
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();
          assertEquals(1, caretCalls);
        });

    test('keydown play shortcut hit-tests pending position', async () => {
      app.onLineFocusContentPositionChange(10, 20, 85);
      assertEquals(0, caretCalls);

      document.dispatchEvent(new KeyboardEvent('keydown', {key: 'k'}));
      await microtasksFinished();

      assertEquals(1, caretCalls);
      assertEquals(85, caretY);
    });

    test('visual move updates pending hit test', async () => {
      app.onLineFocusContentPositionChange(10, 20, 50);
      // E.g. a font size change recenters the snapped line.
      app.onLineFocusVisualPositionChange(40, 20, 80);

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(1, caretCalls);
      assertEquals(80, caretY);
    });

    test(
        'non-empty selection after line focus clears pending hit test',
        async () => {
          contentController.setState(ContentType.HAS_CONTENT);
          await microtasksFinished();

          app.onLineFocusContentPositionChange(10, 20, 50);
          assertEquals(0, caretCalls);

          // Simulate a non-empty selection change.
          selectionController.hasSelection = () => true;
          selectionController.getCurrentSelectionStart = (): ContentPosition =>
              ({
                node: document.body,
                offset: 2,
                source: ContentPositionSource.SELECTION,
              });

          document.dispatchEvent(new Event('selectionchange'));
          await microtasksFinished();

          // Play starts from the selection, so the older line focus point is
          // dropped without a hit test.
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();

          assertEquals(0, caretCalls);
        });

    test(
        'empty selection after line focus does not clear pending hit test',
        async () => {
          contentController.setState(ContentType.HAS_CONTENT);
          await microtasksFinished();

          app.onLineFocusContentPositionChange(10, 20, 50);
          assertEquals(0, caretCalls);

          // Simulate a collapsed/empty selection change.
          selectionController.hasSelection = () => false;

          document.dispatchEvent(new Event('selectionchange'));
          await microtasksFinished();

          // Play still hit-tests the pending line focus position.
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();

          assertEquals(1, caretCalls);
        });

    test('line focus move after selection is hit-tested on play', async () => {
      contentController.setState(ContentType.HAS_CONTENT);
      await microtasksFinished();

      selectionController.hasSelection = () => true;
      selectionController.getCurrentSelectionStart = (): ContentPosition => ({
        node: document.body,
        offset: 2,
        source: ContentPositionSource.SELECTION,
      });
      document.dispatchEvent(new Event('selectionchange'));
      await microtasksFinished();

      // E.g. the panel scrolls to a selection made on the main page.
      app.onLineFocusContentPositionChange(10, 20, 50);
      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(1, caretCalls);
      assertEquals(50, caretY);
    });

    test('manual scroll defers hit testing until play', async () => {
      document.body.style.height = '400px';
      app.style.height = '400px';
      const paragraph = document.createElement('p');
      paragraph.style.whiteSpace = 'pre';
      paragraph.style.lineHeight = '30px';
      paragraph.textContent =
          Array.from({length: 30}, (_, i) => `Line ${i}`).join('\n');
      app.$.container.replaceChildren(paragraph);
      contentController.setState(ContentType.HAS_CONTENT);
      emitEvent(
          app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
          {detail: {data: LineFocusMovement.STATIC}});
      await microtasksFinished();

      const scrollEnd = eventToPromise('scrollend', app.$.containerScroller);
      app.$.containerScroller.scrollTop += 100;
      await scrollEnd;
      await microtasksFinished();
      assertEquals(0, caretCalls);

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();
      assertEquals(1, caretCalls);
    });

    test('settings changes do not hit test', async () => {
      emitEvent(app, ToolbarEvent.FONT_SIZE, {detail: {data: true}});
      emitEvent(app, ToolbarEvent.LINE_SPACING, {detail: {data: 3}});
      emitEvent(app, ToolbarEvent.LETTER_SPACING, {detail: {data: 3}});
      await microtasksFinished();

      assertEquals(0, caretCalls);
    });

    test(
        'visual move without a pending position does not hit test',
        async () => {
          // Drop the position set when line focus was turned on, e.g. after
          // pausing speech.
          speechController.onLineFocusChange(null);

          // E.g. a settings change recenters the focus window.
          app.onLineFocusVisualPositionChange(40, 20, 80);
          emitEvent(app, ToolbarEvent.PLAY_PAUSE);
          await microtasksFinished();

          assertEquals(0, caretCalls);
        });

    test('disabling line focus clears pending hit test', async () => {
      app.onLineFocusContentPositionChange(10, 20, 50);
      assertEquals(0, caretCalls);

      emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: false}});
      await microtasksFinished();

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(0, caretCalls);
    });

    test('showLoading clears pending hit test', async () => {
      app.onLineFocusContentPositionChange(10, 20, 50);
      assertEquals(0, caretCalls);

      app.showLoading();
      await microtasksFinished();

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(0, caretCalls);
    });

    test('pausing active speech does not perform hit test', async () => {
      app.onLineFocusContentPositionChange(10, 20, 50);
      speechController.isSpeechActive = () => true;

      emitEvent(app, ToolbarEvent.PLAY_PAUSE);
      await microtasksFinished();

      assertEquals(0, caretCalls);
    });
  });

  test(
      'switching from static to cursor on single line title focuses title',
      async () => {
        document.body.style.height = '400px';
        app.style.height = '400px';
        const heading = document.createElement('h1');
        heading.style.whiteSpace = 'pre';
        heading.textContent = 'Single line title';
        app.$.container.replaceChildren(heading);
        contentController.setState(ContentType.HAS_CONTENT);
        await microtasksFinished();

        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_STYLE,
            {detail: {data: LineFocusStyle.UNDERLINE}});
        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.STATIC}});
        emitEvent(app, ToolbarEvent.LINE_FOCUS_TOGGLE, {detail: {data: true}});
        await microtasksFinished();
        app.$.containerScroller.scrollTop =
            app.$.containerScroller.scrollHeight;

        emitEvent(
            app, ToolbarEvent.LINE_FOCUS_MOVEMENT,
            {detail: {data: LineFocusMovement.CURSOR}});
        await microtasksFinished();

        assertEquals(0, getLineFocusPadding());
        const cursorY =
            Number.parseFloat(app.style.getPropertyValue('--line-focus-y'));
        const titleBottom =
            calculateTextBounds(
                app.$.container, app.$.appFlexParent.clientHeight)
                .bounds[0]!.bottom;
        assertLT(Math.abs(titleBottom - cursorY), 1);
      });
});
