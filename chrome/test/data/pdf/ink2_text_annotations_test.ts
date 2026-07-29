// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DEFAULT_TEXTBOX_WIDTH, Ink2Manager, PdfViewerPrivateProxyImpl, TextBoxState, TextStyle, Viewport} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkTextAnnotationsElement, TextAnnotationMessageData} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertPositionAndSize, dragHandle, verifyFinishTextAnnotationMessage} from './ink2_text_box_test_utils.js';
import {TestPdfViewerPrivateProxy} from './test_pdf_viewer_private_proxy.js';
import {createMockPdfPluginForTest, createWheelEvent, getTestAnnotation, MockDocumentDimensions, setUpInkTestContext} from './test_util.js';
import type {MockPdfPluginElement} from './test_util.js';

const DEFAULT_HEIGHT = 24;
const FONT_SIZE = 12;
const CLICK_OFFSET = FONT_SIZE / 2;

interface TestContext {
  manager: Ink2Manager;
  viewport: Viewport;
  mockPlugin: MockPdfPluginElement;
}

function setUpTest(): TestContext {
  Ink2Manager.setInstance(null);
  const context = setUpInkTestContext();
  const viewport = context.viewport;
  const mockPlugin = context.mockPlugin;

  const privateProxy = new TestPdfViewerPrivateProxy();
  PdfViewerPrivateProxyImpl.setInstance(privateProxy);

  return {
    manager: Ink2Manager.getInstance(),
    viewport,
    mockPlugin,
  };
}

function createAnnotationsElement(viewport: Viewport):
    InkTextAnnotationsElement {
  document.body.innerHTML = '';
  const annotationsElement = document.createElement('ink-text-annotations');
  annotationsElement.viewport = viewport;
  viewport.setViewportChangedCallback(
      () => annotationsElement.viewportChanged());
  document.body.appendChild(annotationsElement);
  return annotationsElement;
}

function getPlaceholders(annotationsElement: InkTextAnnotationsElement):
    NodeListOf<HTMLElement> {
  return annotationsElement.shadowRoot.querySelectorAll('.placeholder');
}

chrome.test.runTests([
  async function testPlaceholdersRenderedAndSorted() {
    const {manager, viewport} = setUpTest();

    // Add a second page to the document dimensions to support pageIndex 1.
    const dimensions = new MockDocumentDimensions(0, 0);
    dimensions.addPage(400, 500);  // Page 0: 400x500
    dimensions.addPage(400, 500);  // Page 1: 400x500
    viewport.setDocumentDimensions(dimensions);

    // Commit annotations with SCREEN coordinates. Ink2Manager will convert them
    // to PAGE coordinates.
    // Page 0 screen offset: X=55, Y=3. Page 1 screen offset: X=55, Y=503
    // (at 1.0 zoom).

    // Annotation 1: Page 0, Screen X=105, Y=103 (Page X=50, Y=100)
    const annotation1 = {
      ...getTestAnnotation(1),
      text: 'Annotation 1',
      textBoxRect: {height: 20, locationX: 105, locationY: 103, width: 100},
    };

    // Annotation 2: Page 0, Screen X=105, Y=53 (Page X=50, Y=50)
    const annotation2 = {
      ...getTestAnnotation(2),
      text: 'Annotation 2',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };

    // Annotation 3: Page 1, Screen X=65, Y=513 (Page X=10, Y=10)
    const annotation3 = {
      ...getTestAnnotation(3),
      pageIndex: 1,
      text: 'Annotation 3',
      textBoxRect: {height: 20, locationX: 65, locationY: 513, width: 100},
    };

    // Commit them to manager (out of order)
    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation3, true, []);
    manager.commitTextAnnotation(annotation2, true, []);

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(3, placeholders.length);

    // Verify A11y attributes on inner container
    const container = annotationsElement.shadowRoot.querySelector('#container');
    assert(container);
    chrome.test.assertEq('list', container.getAttribute('role'));
    chrome.test.assertEq(
        loadTimeData.getString('ink2TextAnnotationsAxLabel'),
        container.getAttribute('aria-label'));

    // Verify A11y attributes on placeholders
    chrome.test.assertEq('listitem', placeholders[0]!.getAttribute('role'));
    chrome.test.assertEq('0', placeholders[0]!.getAttribute('tabindex'));

    // Verify sorting: annotation2 (Page 0, Y=50) -> annotation1 (Page 0, Y=100)
    // -> annotation3 (Page 1)
    chrome.test.assertEq(
        'Annotation 2', placeholders[0]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 1', placeholders[1]!.getAttribute('aria-label'));
    chrome.test.assertEq(
        'Annotation 3', placeholders[2]!.getAttribute('aria-label'));

    // Assert computed style which includes the CSS offsets (matching
    // ink-text-box): Left: 105 - 12 = 93px Top: 53 - 10 = 43px Width: 100 + 24
    // = 124px Height: 20 + 20 = 40px
    const style2 = window.getComputedStyle(placeholders[0]!);
    chrome.test.assertEq('93px', style2.left);
    chrome.test.assertEq('43px', style2.top);
    chrome.test.assertEq('124px', style2.width);
    chrome.test.assertEq('40px', style2.height);
    chrome.test.assertEq('2', style2.zIndex);

    const style1 = window.getComputedStyle(placeholders[1]!);
    chrome.test.assertEq('1', style1.zIndex);

    const style3 = window.getComputedStyle(placeholders[2]!);
    chrome.test.assertEq('3', style3.zIndex);

    chrome.test.succeed();
  },

  async function testPlaceholderFocusedScroll() {
    const {manager, viewport, mockPlugin} = setUpTest();

    // Commit 3 annotations:
    // 1. Out of viewport in y direction after zoom: page (50, 400), screen
    // (105, 403).
    // 2. Out of viewport in x direction after zoom: page (250, 100), screen
    // (305, 103).
    // 3. Still in viewport after zoom: page (50, 50), screen (105, 53).
    //
    // Sorted order (top-to-bottom, left-to-right):
    // - Annotation 3 (Page Y=50) -> Placeholder 0
    // - Annotation 2 (Page Y=100) -> Placeholder 1
    // - Annotation 1 (Page Y=400) -> Placeholder 2

    const annotation1 = {
      ...getTestAnnotation(1),
      text: 'Scroll Y',
      textBoxRect: {height: 20, locationX: 105, locationY: 403, width: 100},
    };
    const annotation2 = {
      ...getTestAnnotation(2),
      text: 'Scroll X',
      textBoxRect: {height: 20, locationX: 305, locationY: 103, width: 100},
    };
    const annotation3 = {
      ...getTestAnnotation(3),
      text: 'No Scroll',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };

    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation2, true, []);
    manager.commitTextAnnotation(annotation3, true, []);

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    // Zoom to 2.0.
    viewport.setZoom(2.0);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(3, placeholders.length);

    // --- Case 1: Focus Annotation 3 (No Scroll) ---
    // Textbox is at Page X = 50, Page Y = 50.
    // At 2.0x: screenRect is X: [110, 310], Y: [106, 146].
    // This is within viewport 500x500, so no scroll should occur.
    mockPlugin.clearMessages();
    placeholders[0]!.focus();
    placeholders[0]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    let syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertEq(undefined, syncScrollMessage);

    // --- Case 2: Focus Annotation 2 (Scroll X) ---
    // Textbox Page X = 250, Page Y = 100.
    // At 2.0x zoom: screenRect.locationX = 510, width = 200.
    // screenRect.locationY = 206, height = 40.
    // Scrolls X to 305 (clamped from 460). Y remains 0.
    mockPlugin.clearMessages();
    placeholders[1]!.focus();
    placeholders[1]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    chrome.test.assertEq(305, syncScrollMessage.x);
    chrome.test.assertEq(0, syncScrollMessage.y);

    // Reset viewport position to (0,0) for next test.
    viewport.setPosition({x: 0, y: 0});
    await microtasksFinished();

    // --- Case 3: Focus Annotation 1 (Scroll Y) ---
    // Textbox Page X = 50, Page Y = 400.
    // At 2.0x zoom: screenRect.locationX = 110, width = 200.
    // screenRect.locationY = 806, height = 40.
    // Scrolls Y to 505 (clamped from 756). X remains 0.
    mockPlugin.clearMessages();
    placeholders[2]!.focus();
    placeholders[2]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();

    syncScrollMessage =
        mockPlugin.findMessage<{type: string, x: number, y: number}>(
            'syncScrollToRemote');
    chrome.test.assertTrue(syncScrollMessage !== undefined);
    chrome.test.assertEq('syncScrollToRemote', syncScrollMessage.type);
    chrome.test.assertEq(0, syncScrollMessage.x);
    chrome.test.assertEq(505, syncScrollMessage.y);

    chrome.test.succeed();
  },

  async function testViewportChangeReposition() {
    const {manager, viewport} = setUpTest();
    // For page coordinates of (50, 50), set screen coordinates (105, 53) due
    // to 55, 3 offsets.
    const annotation = {
      ...getTestAnnotation(1),
      text: 'Test',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };
    manager.commitTextAnnotation(annotation, true, []);

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    let placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(1, placeholders.length);
    // Offset is 10px inline padding + 2px border --> left = 105 - 12 = 93
    chrome.test.assertEq(
        '93px', window.getComputedStyle(placeholders[0]!).left);

    // Zoom to 2.0
    // Page 0 dimensions at 2.0 zoom: pageX=10, pageY=6, width=780, height=980
    // Screen coords should be:
    // X = 50 * 2 + 10 = 110
    // Y = 50 * 2 + 6 = 106
    // W = 100 * 2 = 200
    // H = 20 * 2 = 40
    viewport.setZoom(2.0);
    await microtasksFinished();

    placeholders = getPlaceholders(annotationsElement);
    const styleAt2 = window.getComputedStyle(placeholders[0]!);
    // Left: 110 - 12 = 98px
    // Y offset is 8px vertical padding + 2px border = 10
    // Top: 106 - 10 = 96px
    // Width: 200 + (24 = 2 * padding + 2 * border) = 224px
    // Height: 40 + (20 = 2 * padding + 2 * border) = 60px
    chrome.test.assertEq('98px', styleAt2.left);
    chrome.test.assertEq('96px', styleAt2.top);
    chrome.test.assertEq('224px', styleAt2.width);
    chrome.test.assertEq('60px', styleAt2.height);

    chrome.test.succeed();
  },

  // Regression test for crbug.com/519251246
  async function testClickOldPositionAfterMove() {
    const {manager, viewport, mockPlugin} = setUpTest();
    await manager.initializeTextAnnotations();

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const textbox = annotationsElement.$.textBox;

    // The textbox uses screen coordinates, but the CSS styles apply offsets.
    // X offset: --offset-x (17px)
    // Y offset: --offset-y (15px)
    // Width offset: border (2 * 1px) + padding (2 * 6px) + X offsets (2 * 5px)
    // = 24px
    // Height offset: border (2 * 1px) + padding (2 * 4px) + Y offsets (2 * 5px)
    // = 20px
    // Matched to chrome/browser/resources/pdf/elements/ink_text_box.css styles.
    const OFFSET_X = 17;
    const OFFSET_Y = 15;
    const WIDTH_OFFSET = 24;
    const HEIGHT_OFFSET = 20;

    const pageRect = viewport.getPageScreenRect(0);

    // (1) Create Box A at 95, 60 in screen coordinates (40, 57 in page
    // coordinates). Since the current text size is 12, the y position of the
    // click is offset by 12 / 2 = 6, resulting in a click at 95, 66 screen (40,
    // 63 page).
    const clickXA = 95;
    const clickYA = 66;
    const createdA =
        await manager.initializeTextAnnotation({x: clickXA, y: clickYA});
    chrome.test.assertTrue(createdA, 'Failed to initialize Box A');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // locationX_ is set to pageX (40).
    // locationY_ is set to pageY - clickOffset = 63 - 6 = 60.
    // Left: locationX_ (95) - OFFSET_X (17) = 78px.
    // Top: locationY_ (60) - OFFSET_Y (15) = 45px.
    let expectedLeft = clickXA - OFFSET_X;
    let expectedTop = clickYA - CLICK_OFFSET - OFFSET_Y;
    const expectedWidthStyle = `${DEFAULT_TEXTBOX_WIDTH + WIDTH_OFFSET}px`;
    const expectedHeightStyle = `${DEFAULT_HEIGHT + HEIGHT_OFFSET}px`;
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeft}px`,
        `${expectedTop}px`);

    textbox.$.textbox.value = 'Annotation A';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (2) Move it to a new position by dragging 100px in both directions.
    await dragHandle(textbox, 100, 100);
    expectedLeft += 100;
    expectedTop += 100;

    // New screen coordinates: x = 95 + 100 = 195, y = 60 + 100 = 160.
    // Left: 195 - OFFSET_X (17) = 178px. Top: 160 - OFFSET_Y (15) = 145px.
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeft}px`,
        `${expectedTop}px`);

    // (3) Initialize a new box (Box B) using the manager with a click position
    // in the original center of Box A.
    // Center X = locationX_ (95) + DEFAULT_TEXTBOX_WIDTH (222) / 2 = 206.
    // Center Y = locationY_ (60) + DEFAULT_HEIGHT (24) / 2 = 72.
    const clickXB = clickXA + DEFAULT_TEXTBOX_WIDTH / 2;
    const clickYB = (clickYA - CLICK_OFFSET) + DEFAULT_HEIGHT / 2;

    mockPlugin.clearMessages();
    const createdB =
        await manager.initializeTextAnnotation({x: clickXB, y: clickYB});
    chrome.test.assertTrue(createdB, 'Failed to initialize Box B');
    await microtasksFinished();

    // (4) Validate that the ink-text-box commits Box A at the moved position.
    // Moved page coordinates:
    // locationX = (195 (screenX) - 55 (pageRect.x)) / 1.0 = 140.
    // locationY = (160 (screenY) - 3 (pageRect.y)) / 1.0 = 157.
    const expectedAnnotationA = getTestAnnotation(0);
    expectedAnnotationA.textBoxRect = {
      locationX: clickXA + 100 - pageRect.x,
      locationY: clickYA - CLICK_OFFSET + 100 - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    };
    expectedAnnotationA.text = 'Annotation A';
    expectedAnnotationA.textAttributes.color = {r: 0, g: 0, b: 0};
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotationA, true);
    mockPlugin.clearMessages();

    // (5) Validate that the ink-text-box moves to the new position of Box B,
    // and shows empty text.
    // Box B screen coordinates:
    // x = 206 -> locationX_ = 206.
    // y = 72 - 6 (clickOffset) = 66 -> locationY_ = 66.
    // Left: 206 - OFFSET_X (17) = 189px. Top: 66 - OFFSET_Y (15) = 51px.
    const expectedLeftB = clickXB - OFFSET_X;
    const expectedTopB = clickYB - CLICK_OFFSET - OFFSET_Y;
    assertPositionAndSize(
        textbox, expectedWidthStyle, expectedHeightStyle, `${expectedLeftB}px`,
        `${expectedTopB}px`);
    chrome.test.assertEq('', textbox.$.textbox.value);

    // (6) Enter some text in Box B.
    textbox.$.textbox.value = 'Annotation B';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (7) Validate that deactivating the text box commits Box B with a
    // different ID and text content from Box A.
    const stateChanged = eventToPromise('state-changed', textbox);
    await annotationsElement.commitActiveAnnotation();
    await stateChanged;
    await microtasksFinished();

    // Committed page coordinates for Box B:
    // locationX = (206 (screenX) - 55 (pageRect.x)) / 1.0 = 151.
    // locationY = (66 (screenY) - 3 (pageRect.y)) / 1.0 = 63.
    const expectedAnnotationB = getTestAnnotation(1);
    expectedAnnotationB.textBoxRect = {
      locationX: clickXB - pageRect.x,
      locationY: clickYB - CLICK_OFFSET - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    };
    expectedAnnotationB.text = 'Annotation B';
    expectedAnnotationB.textAttributes.color = {r: 0, g: 0, b: 0};
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotationB, true);

    chrome.test.succeed();
  },

  // Tests that when an existing annotation is re-styled, the new styles are
  // correctly committed when a new annotation is activated.
  // Regression test for crbug.com/519251247
  async function testCommitStyleUpdates() {
    const {manager, viewport, mockPlugin} = setUpTest();
    await manager.initializeTextAnnotations();

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const textbox = annotationsElement.$.textBox;

    const pageRect = viewport.getPageScreenRect(0);

    // (1) Initialize a text annotation. Click at screen coordinates 100, 66
    // which locates the annotation at 100, 60 due to the click offset.
    const clickXA = 100;
    const clickYA = 66;
    const createdA =
        await manager.initializeTextAnnotation({x: clickXA, y: clickYA});
    chrome.test.assertTrue(createdA, 'Failed to initialize Box A');
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));

    // (2) Add some text.
    textbox.$.textbox.value = 'Hello';
    textbox.$.textbox.dispatchEvent(new CustomEvent('input'));
    await microtasksFinished();

    // (3) Commit the annotation and validate.
    const expectedAnnotation = getTestAnnotation(0);
    expectedAnnotation.textBoxRect = {
      locationX: clickXA - pageRect.x,
      locationY: clickYA - CLICK_OFFSET - pageRect.y,
      width: DEFAULT_TEXTBOX_WIDTH,
      height: DEFAULT_HEIGHT,
    };
    expectedAnnotation.text = 'Hello';
    expectedAnnotation.textAttributes.color = {r: 0, g: 0, b: 0};

    await textbox.commitTextAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(isVisible(textbox));
    verifyFinishTextAnnotationMessage(mockPlugin, expectedAnnotation, true);
    mockPlugin.clearMessages();

    // (4) Re-activate this annotation.
    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(1, placeholders.length);
    placeholders[0]!.click();
    await microtasksFinished();
    chrome.test.assertTrue(isVisible(textbox));
    chrome.test.assertEq('Hello', textbox.$.textbox.value);

    // (5) Modify the font color and style to red + bold.
    manager.setTextColor({r: 255, g: 0, b: 0});
    manager.setTextStyles({
      [TextStyle.BOLD]: true,
      [TextStyle.ITALIC]: false,
    });
    await microtasksFinished();

    // (6) Initialize a new annotation elsewhere.
    // Click at 200, 206 screen -> top-left at 200, 200 screen after offset.
    const createdB = await manager.initializeTextAnnotation({x: 200, y: 206});
    chrome.test.assertTrue(createdB, 'Failed to initialize Box B');
    await microtasksFinished();

    // (7) Validate that the commit message triggered by the new initialization
    // contains the correct styling.
    const expectedAnnotationEdited = structuredClone(expectedAnnotation);
    expectedAnnotationEdited.textAttributes.color = {r: 255, g: 0, b: 0};
    expectedAnnotationEdited.textAttributes.styles[TextStyle.BOLD] = true;
    expectedAnnotationEdited.textAttributes.styles[TextStyle.ITALIC] = false;

    verifyFinishTextAnnotationMessage(
        mockPlugin, expectedAnnotationEdited, true);

    chrome.test.succeed();
  },

  async function testInitializeTextBoxForwarding() {
    const {manager, viewport} = setUpTest();

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    // Initially, textbox should be hidden.
    chrome.test.assertTrue(annotationsElement.$.textBox.hidden);

    // Manually dispatch 'initialize-text-box' on manager.
    const annotation = getTestAnnotation(1);
    annotation.textBoxRect.height = DEFAULT_HEIGHT;
    annotation.textBoxRect.width = DEFAULT_TEXTBOX_WIDTH;
    const expectedPageDimensions = viewport.getPageScreenRect(0);
    manager.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation,
        pageDimensions: expectedPageDimensions,
      },
    }));
    await microtasksFinished();

    // Verify that the textbox child is visible and has correct content.
    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    chrome.test.assertEq(
        annotation.text, annotationsElement.$.textBox.$.textbox.value);

    // Verify the position and size.
    assertPositionAndSize(
        annotationsElement.$.textBox, '246px', '44px', '43px', '10px');

    chrome.test.succeed();
  },

  async function testPlaceholdersTabindexWithActiveAnnotation() {
    const {manager, viewport} = setUpTest();

    // Add one annotation so a placeholder is in the DOM.
    const annotation = {
      ...getTestAnnotation(1),
      text: 'Test',
      textBoxRect: {height: 20, locationX: 105, locationY: 53, width: 100},
    };
    manager.commitTextAnnotation(annotation, true, []);

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(1, placeholders.length);

    // Initially, tabindex should be 0.
    chrome.test.assertEq('0', placeholders[0]!.getAttribute('tabindex'));

    // Activate the annotation manually.
    const activeAnnotation = getTestAnnotation(2);
    const pageDimensions = viewport.getPageScreenRect(0);
    manager.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation: activeAnnotation,
        pageDimensions,
      },
    }));
    await microtasksFinished();

    // Now tabindex should be -1.
    chrome.test.assertEq('-1', placeholders[0]!.getAttribute('tabindex'));

    // Deactivate the annotation (simulate via event).
    annotationsElement.$.textBox.dispatchEvent(
        new CustomEvent('state-changed', {
          detail: TextBoxState.INACTIVE,
        }));
    await microtasksFinished();

    // Tabindex should be back to 0.
    chrome.test.assertEq('0', placeholders[0]!.getAttribute('tabindex'));

    chrome.test.succeed();
  },

  async function testCommitActiveAnnotationOnNewInit() {
    const {manager, viewport, mockPlugin} = setUpTest();

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    // 1. Initialize Annotation A (id=1).
    const annotationA = {...getTestAnnotation(1), text: 'Annotation A'};
    const pageDimensions = viewport.getPageScreenRect(0);
    manager.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation: annotationA,
        pageDimensions,
      },
    }));
    await microtasksFinished();

    // Verify Annotation A is active in the textbox.
    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    chrome.test.assertEq(
        'Annotation A', annotationsElement.$.textBox.$.textbox.value);

    // 2. Simulate user editing the text.
    annotationsElement.$.textBox.$.textbox.value = 'Annotation A Edited';
    annotationsElement.$.textBox.$.textbox.dispatchEvent(new Event('input'));
    await microtasksFinished();

    // 3. Initialize Annotation B (id=2).
    // This should trigger the parent to commit Annotation A first.
    mockPlugin.clearMessages();
    const annotationB = {...getTestAnnotation(2), text: 'Annotation B'};
    manager.dispatchEvent(new CustomEvent('initialize-text-box', {
      detail: {
        annotation: annotationB,
        pageDimensions,
      },
    }));
    await microtasksFinished();

    // 4. Verify that Annotation A was committed.
    const finishMessage =
        mockPlugin.findMessage<{type: string, data: TextAnnotationMessageData}>(
            'finishTextAnnotation');
    chrome.test.assertTrue(finishMessage !== undefined);
    chrome.test.assertEq('finishTextAnnotation', finishMessage.type);
    chrome.test.assertEq(1, finishMessage.data.id);
    chrome.test.assertEq('Annotation A Edited', finishMessage.data.text);
    chrome.test.assertTrue(finishMessage.data.isEdited);

    // 5. Verify that Annotation B is now active in the textbox.
    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    chrome.test.assertEq(
        'Annotation B', annotationsElement.$.textBox.$.textbox.value);

    chrome.test.succeed();
  },

  async function testActivatePlaceholder() {
    const {manager, viewport, mockPlugin} = setUpTest();

    // Add one annotation to create a placeholder.
    const testAnnotation = getTestAnnotation(0);
    testAnnotation.text = 'Hello World';
    testAnnotation.textBoxRect.width = DEFAULT_TEXTBOX_WIDTH;
    testAnnotation.textBoxRect.height = DEFAULT_HEIGHT;
    // Position: x=60, y=25, w=200, h=24. Page offsets: x=55, y=3.
    // Screen position: x=115, y=28, w=200, h=24.
    mockPlugin.setMessageReply('getAllTextAnnotations', {
      annotations: [testAnnotation],
    });
    await manager.initializeTextAnnotations();

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(1, placeholders.length);
    const placeholder = placeholders[0]!;

    function verifyEditTextAnnotation(expected: boolean, id: number = 0) {
      const editTextAnnotationMessage =
          mockPlugin.findMessage<{type: string, data: number}>(
              'editTextAnnotation');
      chrome.test.assertEq(expected, editTextAnnotationMessage !== undefined);
      if (expected) {
        chrome.test.assertEq(
            'editTextAnnotation', editTextAnnotationMessage!.type);
        chrome.test.assertEq(id, editTextAnnotationMessage!.data);
      }
    }

    // 1. Verify click activates it.
    mockPlugin.clearMessages();
    placeholder.click();
    await microtasksFinished();

    // Verify textbox is active with the correct annotation (in screen coords).
    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    const activeAnnotation1 = annotationsElement.$.textBox.annotation;
    chrome.test.assertTrue(activeAnnotation1 !== null);
    chrome.test.assertEq(testAnnotation.id, activeAnnotation1.id);
    chrome.test.assertEq(testAnnotation.text, activeAnnotation1.text);
    // Screen coords: x = 60 + 55 = 115, y = 25 + 3 = 28.
    chrome.test.assertEq(115, activeAnnotation1.textBoxRect.locationX);
    chrome.test.assertEq(28, activeAnnotation1.textBoxRect.locationY);
    chrome.test.assertEq(
        DEFAULT_TEXTBOX_WIDTH, activeAnnotation1.textBoxRect.width);
    chrome.test.assertEq(DEFAULT_HEIGHT, activeAnnotation1.textBoxRect.height);
    // Verify manager was notified.
    verifyEditTextAnnotation(true, testAnnotation.id);

    // Deactivate it.
    annotationsElement.$.textBox.dispatchEvent(
        new CustomEvent('state-changed', {
          detail: TextBoxState.INACTIVE,
        }));
    await microtasksFinished();
    chrome.test.assertTrue(annotationsElement.$.textBox.hidden);

    // 2. Verify Keyboard (Enter) activates it.
    mockPlugin.clearMessages();
    placeholder.focus();
    placeholder.dispatchEvent(new KeyboardEvent('keydown', {key: 'Enter'}));
    await microtasksFinished();

    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    const activeAnnotation2 = annotationsElement.$.textBox.annotation;
    chrome.test.assertTrue(activeAnnotation2 !== null);
    chrome.test.assertEq(testAnnotation.id, activeAnnotation2.id);
    verifyEditTextAnnotation(true, testAnnotation.id);

    // Deactivate it.
    annotationsElement.$.textBox.dispatchEvent(
        new CustomEvent('state-changed', {
          detail: TextBoxState.INACTIVE,
        }));
    await microtasksFinished();
    chrome.test.assertTrue(annotationsElement.$.textBox.hidden);

    // 3. Verify Keyboard (Space) activates it.
    mockPlugin.clearMessages();
    placeholder.focus();
    placeholder.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    await microtasksFinished();

    chrome.test.assertFalse(annotationsElement.$.textBox.hidden);
    const activeAnnotation3 = annotationsElement.$.textBox.annotation;
    chrome.test.assertTrue(activeAnnotation3 !== null);
    chrome.test.assertEq(testAnnotation.id, activeAnnotation3.id);
    verifyEditTextAnnotation(true, testAnnotation.id);

    chrome.test.succeed();
  },

  async function testPlaceholderFocusedScroll3Pages() {
    const {manager, viewport} = setUpTest();

    const dimensions = new MockDocumentDimensions(0, 0);
    dimensions.addPage(400, 500);  // Page 0: 400x500
    dimensions.addPage(400, 500);  // Page 1: 400x500
    dimensions.addPage(400, 500);  // Page 2: 400x500
    viewport.setDocumentDimensions(dimensions);

    const annotation1 = {
      ...getTestAnnotation(1),
      pageIndex: 0,
      text: 'Page 1 Annotation',
      // Rectangle is in screen coordinates. y = 403 is on page 1 at 400.
      textBoxRect: {height: 20, locationX: 105, locationY: 403, width: 100},
    };

    const annotation2 = {
      ...getTestAnnotation(2),
      pageIndex: 1,
      text: 'Page 2 Annotation',
      // Rectangle is in screen coordinates. y = 603 is on page 2 at 100.
      textBoxRect: {height: 20, locationX: 105, locationY: 603, width: 100},
    };

    const annotation3 = {
      ...getTestAnnotation(3),
      pageIndex: 2,
      text: 'Page 3 Annotation',
      // Rectangle is in screen coordinates. y = 1103 is on page 3 at 100.
      textBoxRect: {height: 20, locationX: 105, locationY: 1103, width: 100},
    };

    manager.commitTextAnnotation(annotation1, true, []);
    manager.commitTextAnnotation(annotation2, true, []);
    manager.commitTextAnnotation(annotation3, true, []);

    const annotationsElement = createAnnotationsElement(viewport);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(3, placeholders.length);

    placeholders[0]!.focus();
    placeholders[0]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();
    chrome.test.assertEq(0, viewport.position.y);

    placeholders[1]!.focus();
    placeholders[1]!.dispatchEvent(new FocusEvent('focus'));
    await microtasksFinished();
    chrome.test.assertEq(553, viewport.position.y);

    const rect1 = placeholders[0]!.getBoundingClientRect();
    const rect2 = placeholders[1]!.getBoundingClientRect();
    const rect3 = placeholders[2]!.getBoundingClientRect();

    // Annotation 1 is off screen, annotation 2 is near top left of the
    // viewport, and annotation 3 is still beyond the end of the viewport.
    chrome.test.assertEq(-160, rect1.top);
    chrome.test.assertEq(40, rect2.top);
    chrome.test.assertEq(540, rect3.top);

    chrome.test.assertEq(0, annotationsElement.scrollTop);
    chrome.test.assertEq(0, annotationsElement.$.container.scrollTop);
    chrome.test.assertEq(0, document.body.scrollTop);
    chrome.test.assertEq(0, document.documentElement.scrollTop);

    chrome.test.succeed();
  },

  async function testWheelOnAnnotationScrolls() {
    // Create real DOM elements instead of using MockElement to test native
    // event bubbling from the annotation placeholder up to the scroll
    // container.
    document.body.innerHTML = getTrustedHTML`
      <div id="main">
        <div id="container" style="width: 100px; height: 100px;">
          <div id="sizer"></div>
          <div id="content"></div>
        </div>
      </div>
    `;

    const container = document.body.querySelector<HTMLElement>('#container')!;
    const sizer = document.body.querySelector<HTMLElement>('#sizer')!;
    const content = document.body.querySelector<HTMLElement>('#content')!;

    const viewport = new Viewport(container, sizer, content, 15, 1);

    const mockPlugin = createMockPdfPluginForTest();
    mockPlugin.viewport = viewport;
    viewport.setRemoteContent(mockPlugin);

    // Set document dimensions larger than the container to make it scrollable.
    const documentDimensions = new MockDocumentDimensions();
    documentDimensions.addPage(100, 1000);
    viewport.setDocumentDimensions(documentDimensions);

    // Initialize the annotations manager and UI element.
    Ink2Manager.setInstance(null);
    const manager = Ink2Manager.getInstance();
    manager.setViewport(viewport);
    await manager.initializeTextAnnotations();

    const annotationsElement = document.createElement('ink-text-annotations');
    annotationsElement.viewport = viewport;
    viewport.setViewportChangedCallback(
        () => annotationsElement.viewportChanged());
    container.appendChild(annotationsElement);

    // Add a placeholder text annotation.
    const annotation = {
      ...getTestAnnotation(1),
      text: 'Test Annotation',
      textBoxRect: {height: 50, locationX: 10, locationY: 10, width: 50},
    };
    manager.commitTextAnnotation(annotation, true, []);
    await microtasksFinished();

    const placeholders = getPlaceholders(annotationsElement);
    chrome.test.assertEq(1, placeholders.length);
    const placeholder = placeholders[0]!;

    // Dispatch a wheel event on the placeholder. It should bubble up.
    mockPlugin.clearMessages();
    placeholder.dispatchEvent(
        createWheelEvent(50, {clientX: 0, clientY: 0}, false));

    // Assert that the wheel event was intercepted and forwarded to the plugin
    // as a syncScrollToRemote message.
    const message = mockPlugin.findMessage('syncScrollToRemote');
    chrome.test.assertTrue(!!message);
    const scrollMessage = message as unknown as {x: number, y: number};
    chrome.test.assertEq(50, scrollMessage.y);

    // Cleanup DOM.
    document.body.innerHTML = '';
    chrome.test.succeed();
  },
]);
