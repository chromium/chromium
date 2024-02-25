// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// out/Debug/browser_tests \
//    --gtest_filter=ReadAnythingAppReadAloudTest.ReadAloud_LinksToggleButtonOnToolbar

// Do not call the real `onConnected()`. As defined in
// ReadAnythingAppController, onConnected creates mojo pipes to connect to the
// rest of the Read Anything feature, which we are not testing here.
(() => {
  chrome.readingMode.onConnected = () => {};

  const readAnythingApp = document.querySelector('read-anything-app');
  const container = readAnythingApp.shadowRoot.getElementById('container');
  const toolbar =
      readAnythingApp.shadowRoot.querySelector('read-anything-toolbar')
          .shadowRoot;
  const playPauseButton = toolbar.getElementById('play-pause');

  let result = true;
  const assertEquals = (actual, expected) => {
    const isEqual = actual === expected;
    if (!isEqual) {
      console.error(
          'Expected: ' + JSON.stringify(expected) + ', ' +
          'Actual: ' + JSON.stringify(actual));
    }
    result = result && isEqual;
    return isEqual;
  };

  const assertContainerContainsLinks = () => {
    const innerHTML = container.innerHTML;
    assertEquals(innerHTML.includes('a href'), true);
  };

  const assertContainerDoesNotContainLinks = () => {
    const innerHTML = container.innerHTML;
    assertEquals(innerHTML.includes('a href'), false);
  };

  // root htmlTag='#document' id=1
  // ++link htmlTag='a' url='http://www.google.com' id=2
  // ++++staticText name='This is a link.' id=3
  // ++link htmlTag='a' url='http://www.youtube.com' id=4
  // ++++staticText name='This is another link.' id=5
  const axTree = {
    rootId: 1,
    nodes: [
      {
        id: 1,
        role: 'rootWebArea',
        htmlTag: '#document',
        childIds: [2, 4],
      },
      {
        id: 2,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.google.com',
        childIds: [3],
      },
      {
        id: 3,
        role: 'staticText',
        name: 'This is a link.',
      },
      {
        id: 4,
        role: 'link',
        htmlTag: 'a',
        url: 'http://www.youtube.com',
        childIds: [5],
      },
      {
        id: 5,
        role: 'staticText',
        name: 'This is another link.',
      },
    ],
  };

  chrome.readingMode.setContentForTesting(axTree, [2, 4]);

  const linksButtonIconEnabled = 'read-anything:links-enabled';
  const linksButtonIconDisabled = 'read-anything:links-disabled';

  // Get button.
  const link_toggle_button = toolbar.getElementById('link-toggle-button');
  // Assert enabled state.
  assertContainerContainsLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, false);

  // Play speech.
  playPauseButton.click();

  // Assert links disabled in HTML after speech played. Button should be
  // disabled but the icon should still be the enabled icon.
  assertContainerDoesNotContainLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, true);

  // Pause speech.
  playPauseButton.click();

  // Assert links are enabled again in HTML after speech played.
  assertContainerContainsLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, false);

  // Toggle links.
  link_toggle_button.click();

  // Assert disabled state.
  assertContainerDoesNotContainLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconDisabled);
  assertEquals(link_toggle_button.disabled, false);

  // Play speech.
  playPauseButton.click();

  // Assert links disabled in HTML after speech played.
  assertContainerDoesNotContainLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconDisabled);
  assertEquals(link_toggle_button.disabled, true);

  // Pause speech.
  playPauseButton.click();

  // Assert links remain disabled in HTML after speech played.
  assertContainerDoesNotContainLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconDisabled);
  assertEquals(link_toggle_button.disabled, false);

  // Show links
  link_toggle_button.click();

  // Assert links shown.
  assertContainerContainsLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, false);

  // Play speech.
  playPauseButton.click();

  // Assert links hidden.
  assertContainerDoesNotContainLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, true);

  // Move to the end of page to trigger the end of speech.
  for (let i = 0; i < 6; i++) {
    readAnythingApp.playNextGranularity();
  }

  // Assert that links are shown again after speech finishes.
  assertContainerContainsLinks();
  assertEquals(link_toggle_button.ironIcon, linksButtonIconEnabled);
  assertEquals(link_toggle_button.disabled, false);
  playPauseButton.click();

  return result;
})();
