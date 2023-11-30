// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://manage-mirrorsync/components/manage_mirrorsync.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {BrowserProxy} from 'chrome://manage-mirrorsync/browser_proxy.js';
import {FolderSelector} from 'chrome://manage-mirrorsync/components/folder_selector.js';
import {PageHandlerRemote} from 'chrome://manage-mirrorsync/manage_mirrorsync.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {assertArrayEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

/**
 * A fake BrowserProxy implementation that enables switching out the real one to
 * mock various mojo responses.
 */
class ManageMirrorSyncTestBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  constructor() {
    super(['getChildFolders']);
    this.handler = TestMock.fromClass(PageHandlerRemote);
  }
}

/**
 * Wait until the supplied function evaluates to true, repeating evaluation
 * every 100ms for a total time of 5s.
 */
async function waitUntil(func: () => boolean) {
  let promiseResolve: (value: null) => void;
  let promiseReject: (error: Error) => void;
  const promise = new Promise((resolve, reject) => {
    promiseResolve = resolve;
    promiseReject = reject;
  });
  const interval = setInterval(() => {
    if (func()) {
      clearInterval(interval);
      promiseResolve(null);
    }
  }, 100);
  setTimeout(() => {
    clearInterval(interval);
    promiseReject(new Error('waitUntil has timed out'));
  }, 5000);
  return promise;
}

/**
 * Helper to sanitize a path by ensuring double quote characters are properly
 * escaped.
 */
function sanitizePath(path: string): string {
  return path.replace(/"/g, '\\"');
}

suite('<manage-mirrorsync>', () => {
  /* Holds the <manage-mirrorsync> app */
  let appHolder: HTMLDivElement;
  /* The <manage-mirrorsync> app, this gets cleared before every test */
  let manageMirrorSyncApp: HTMLElement;
  /* The BrowserProxy element to make assertions on when methods are called */
  let testProxy: ManageMirrorSyncTestBrowserProxy;

  /**
   * Runs prior to all the tests running, attaches a div to enable isolated
   * clearing and attaching of the web component.
   */
  suiteSetup(() => {
    appHolder = document.createElement('div');
    document.body.appendChild(appHolder);
  });

  /**
   * Runs before every test. Ensures the DOM is clear of any existing
   * <manage-mirrorsync> components.
   */
  setup(() => {
    assert(window.trustedTypes);
    appHolder.innerHTML = window.trustedTypes.emptyHTML;
    testProxy = new ManageMirrorSyncTestBrowserProxy();
    BrowserProxy.setInstance(testProxy);
    manageMirrorSyncApp = document.createElement('manage-mirrorsync');
    appHolder.appendChild(manageMirrorSyncApp);
  });

  /**
   * Runs after every test. Removes all elements from the <div> added to hold
   * the <manage-mirrorsync> component.
   */
  teardown(() => {
    appHolder.innerHTML = window.trustedTypes!.emptyHTML;
  });

  /**
   * Helper function to run a querySelector over the MirrorSyncApp shadowRoot
   * and assert non-nullness.
   */
  function queryMirrorSyncShadowRoot(selector: string): HTMLElement|null {
    return manageMirrorSyncApp!.shadowRoot!.querySelector(selector);
  }

  /**
   * Returns the <folder-selector> element on the page.
   */
  function getFolderSelector(): FolderSelector {
    return (queryMirrorSyncShadowRoot('folder-selector')! as FolderSelector);
  }

  /**
   * Queries elements in the <folder-selector> shadowRoot.
   */
  function queryFolderSelectorShadowRoot(selector: string): HTMLElement|null {
    return getFolderSelector().shadowRoot!.querySelector(selector);
  }

  /**
   * Show the <folder-selector> element by selecting the "Sync selected files or
   * folders" button.
   */
  async function showFolderSelector(): Promise<void> {
    // <folder-selection> should be hidden to call this method.
    assertTrue(getFolderSelector().hidden);

    // Click the "Sync selected files or folders" button to show the folder
    // hierarchy web component.
    queryMirrorSyncShadowRoot('#selected')!.click();

    // Ensure the <folder-selector> element is visible after pressing the
    // checkbox.
    await waitUntil(() => getFolderSelector().hidden === false);
  }

  /**
   * Queries all the input elements with data-full-path attributes. Elements
   * with attributes but empty OR are not visible in the viewport are excluded.
   * Returns a string array of the data-full-path values.
   */
  function getAllVisiblePaths(): string[] {
    const elements = getFolderSelector().shadowRoot!.querySelectorAll(
        'input[data-full-path]');
    if (elements.length === 0) {
      return [];
    }
    const paths: string[] = [];
    for (const element of elements) {
      const dataFullPath = element.getAttribute('data-full-path');
      if (!dataFullPath || !isVisible(element)) {
        continue;
      }
      paths.push(dataFullPath!);
    }
    return paths;
  }

  /**
   * Wait until the visible paths are the supplied paths then assert that they
   * actually match the `expectedPaths`.
   */
  async function waitAndAssertVisiblePaths(expectedPaths: string[]):
      Promise<void> {
    await waitUntil(() => getAllVisiblePaths().length === expectedPaths.length);
    assertArrayEquals(getAllVisiblePaths(), expectedPaths);
  }

  function getInputElement(path: string): HTMLInputElement {
    const element = queryFolderSelectorShadowRoot(
        `input[data-full-path="${sanitizePath(path)}"]`);
    assertNotEquals(element, null);
    return (element as HTMLInputElement);
  }

  /**
   * Helper method to expand a particular node in the tree to show all its
   * children.
   */
  function expandPath(path: string) {
    const input = getInputElement(path);
    const liElement = input.parentElement as HTMLLIElement;
    liElement.click();
  }

  test(
      'checking individual folder selection shows folder hierarchy',
      async () => {
        // Set result to return an empty array of paths.
        testProxy.handler.setResultFor('getChildFolders', {paths: []});

        // Show the <folder-selector> element.
        await showFolderSelector();

        // The only rendered path should be the root one.
        await waitAndAssertVisiblePaths(['/']);
      });

  test(
      'children of root should be rendered, but not their descendants',
      async () => {
        // Set result to return an empty array of paths.
        testProxy.handler.setResultFor(
            'getChildFolders',
            {paths: [{path: '/foo'}, {path: '/foo/bar'}, {path: '/baz'}]});

        // Show the <folder-selector> element.
        await showFolderSelector();

        // All top-level paths should be visible on startup, but `/foo/bar`
        // should not be visible.
        await waitAndAssertVisiblePaths(['/', '/foo', '/baz']);
      });

  test('when expanding an element its children should be visible', async () => {
    // Set result to return an empty array of paths.
    testProxy.handler.setResultFor(
        'getChildFolders', {paths: [{path: '/foo'}, {path: '/foo/bar'}]});

    // Show the <folder-selector> element.
    await showFolderSelector();

    // All top-level paths should be visible on startup, but `/foo/bar` should
    // not be visible.
    await waitAndAssertVisiblePaths(['/', '/foo']);

    // Expand the /foo path which should make the /foo/bar path visible.
    expandPath('/foo');

    // After expanding expect that /foo/bar is now visible in the DOM.
    await waitAndAssertVisiblePaths(['/', '/foo', '/foo/bar']);
  });

  test(
      'selecting a path should make its children disabled on expansion',
      async () => {
        // Set result to return an empty array of paths.
        testProxy.handler.setResultFor(
            'getChildFolders', {paths: [{path: '/foo'}, {path: '/foo/bar'}]});

        // Show the <folder-selector> element.
        await showFolderSelector();

        // All top-level paths should be visible on startup, but `/foo/bar`
        // should not be visible.
        await waitAndAssertVisiblePaths(['/', '/foo']);

        // Select /foo then expand /foo.
        getInputElement('/foo').click();
        expandPath('/foo');

        // After selecting and expanding /foo the /foo/bar path should be
        // expanded.
        await waitAndAssertVisiblePaths(['/', '/foo', '/foo/bar']);

        // The path should also already be selected and disabled from selecting
        // as its parent element is already selected.
        assertTrue(getInputElement('/foo/bar').checked);
        assertTrue(getInputElement('/foo/bar').disabled);

        // Only the exact selected paths should be returned, not the descendants
        // (despite being visually shown as checked).
        assertArrayEquals(getFolderSelector().selectedPaths, ['/foo']);
      });
});
