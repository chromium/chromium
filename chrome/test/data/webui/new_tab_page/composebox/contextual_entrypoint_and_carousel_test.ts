// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ContextualEntrypointAndCarouselElement} from 'chrome://new-tab-page/lazy_load.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabPageContextualEntrypointAndCarouselTest', () => {
  let element: ContextualEntrypointAndCarouselElement;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new ContextualEntrypointAndCarouselElement();
    document.body.appendChild(element);
  });

  test(
      'disabling file upload does not show file upload button in menu',
      async () => {
        loadTimeData.overrideValues({
          'composeboxShowContextMenu': true,
          'composeboxShowPdfUpload': false,
        });
        // Re-create the element to pick up the new loadTimeData.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        element = new ContextualEntrypointAndCarouselElement();
        document.body.appendChild(element);
        await microtasksFinished();

        const contextEntrypoint = element.$.contextEntrypoint;
        assertTrue(!!contextEntrypoint);

        const entrypointButton =
            contextEntrypoint.shadowRoot.querySelector<HTMLElement>(
                '#entrypoint');
        assertTrue(isVisible(entrypointButton));
        entrypointButton!.click();
        await microtasksFinished();

        const menu = contextEntrypoint.$.menu;
        assertTrue(menu.open);

        const fileUploadButton = menu.querySelector('#fileUpload');
        assertFalse(!!fileUploadButton);

        const imageUploadButton = menu.querySelector('#imageUpload');
        assertTrue(isVisible(imageUploadButton));
      });

  test('voice search click emits event', async () => {
    element.searchboxLayoutMode = 'TallTopContext';
    element.showDropdown = true;
    element.showVoiceSearch = true;
    await microtasksFinished();

    const whenOpenVoiceSearch = eventToPromise('open-voice-search', element);

    const voiceSearchButton = element.$.voiceSearchButton;
    assertTrue(isVisible(voiceSearchButton));
    voiceSearchButton.click();

    await whenOpenVoiceSearch;
  });

  test('image upload button clicks file input or fires event', async () => {
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': false,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new ContextualEntrypointAndCarouselElement();
    document.body.appendChild(element);
    await microtasksFinished();

    const imageUploadClickEventPromise =
        eventToPromise('click', element.$.imageInput);
    element.$.imageUploadButton.click();
    await imageUploadClickEventPromise;

    element.entrypointName = 'ContextualTasks';
    await microtasksFinished();
    const openFileDialogPromise = eventToPromise('open-file-dialog', element);
    element.$.imageUploadButton.click();
    const event = await openFileDialogPromise;
    assertTrue(event.detail.isImage);
  });

  test('file upload button clicks file input or fires event', async () => {
    loadTimeData.overrideValues({
      'composeboxShowPdfUpload': true,
      'composeboxShowContextMenu': false,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new ContextualEntrypointAndCarouselElement();
    document.body.appendChild(element);
    await microtasksFinished();

    const fileUploadClickEventPromise =
        eventToPromise('click', element.$.fileInput);
    element.$.fileUploadButton.click();
    await fileUploadClickEventPromise;

    element.entrypointName = 'ContextualTasks';
    await microtasksFinished();
    const openFileDialogPromise = eventToPromise('open-file-dialog', element);
    element.$.fileUploadButton.click();
    const event = await openFileDialogPromise;
    assertFalse(event.detail.isImage);
  });

  test('Recent Tab chip respects allowed input types', async () => {
    // Arrange.
    loadTimeData.overrideValues({
      'composeboxShowContextMenu': true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = new ContextualEntrypointAndCarouselElement();
    document.body.appendChild(element);
    await microtasksFinished();

    element.showModelPicker = true;
    element.inputState = {
      allowedModels: [],
      allowedTools: [],
      allowedInputTypes: [InputType.kBrowserTab],
      activeModel: 0,
      activeTool: 0,
      disabledModels: [],
      disabledTools: [],
      disabledInputTypes: [],
      toolConfigs: [],
      modelConfigs: [],
      inputTypeConfigs: [],
      toolsSectionConfig: null,
      modelSectionConfig: null,
      hintText: '',
    };
    element.tabSuggestions = [{
      title: 'Tab 1',
      url: { url: 'https://google.com' },
      tabId: 1,
      showInCurrentTabChip: true,
      showInPreviousTabChip: false,
      lastActive: {internalValue: BigInt(0)},
    }];
    element.showRecentTabChip = true;
    await microtasksFinished();

    // Assert (allowed).
    const recentTabChip =
        element.shadowRoot.querySelector('composebox-recent-tab-chip');
    assertTrue(!!recentTabChip);

    // Act (disallow).
    element.inputState = {
      ...element.inputState,
      allowedInputTypes: [],  // No BrowserTab
    };
    await microtasksFinished();

    // Assert (disallowed).
    const recentTabChipHidden =
        element.shadowRoot.querySelector('composebox-recent-tab-chip');
    assertFalse(!!recentTabChipHidden);
  });
});
