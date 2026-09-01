// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://skills/loading_page.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {ErrorType} from 'chrome://skills/error_page.js';
import {SkillsDialogType} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsWebview} from 'chrome://skills/v2/skills_webview.js';
import type {SkillsWebviewBridge} from 'chrome://skills/v2/skills_webview_bridge.js';
import {IS_SAVING_GEMINI_QUERY_PARAMETER, SkillSource, SOURCE_QUERY_PARAMETER} from 'chrome://skills/v2/skills_webview_bridge_constants.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

class TestSkillsWebview extends SkillsWebview {
  getRemoteUrlForTesting(): string {
    return this.remoteUrl;
  }
}

suite('SkillsWebviewTest', () => {
  setup(() => {
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = {};
    }
  });

  test('SkillsWebview_SavingGeminiQuery', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      dialogType: SkillsDialogType.kAdd,
      skillId: '',
      skillPrompt: 'save this prompt',
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals(
        'true', url.searchParams.get(IS_SAVING_GEMINI_QUERY_PARAMETER));
    assertEquals(null, url.searchParams.get(SOURCE_QUERY_PARAMETER));
  });

  test('SkillsWebview_AddWithId_FirstPartySkill', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      dialogType: SkillsDialogType.kAdd,
      skillId: 'some_id',
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals('some_id', url.searchParams.get('id'));
    assertEquals(
        SkillSource.FIRST_PARTY, url.searchParams.get(SOURCE_QUERY_PARAMETER));
  });

  test('SkillsWebview_EditWithId_UserSkill', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      dialogType: SkillsDialogType.kEdit,
      skillId: 'some_id',
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals('some_id', url.searchParams.get('id'));
    assertEquals(
        SkillSource.USER, url.searchParams.get(SOURCE_QUERY_PARAMETER));
  });

  test('SkillsWebview_LanguageCode', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      languageCode: 'es',
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals('es', url.searchParams.get('hl'));
  });

  test('SkillsWebview_EmptyLanguageCode_OmitHl', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      languageCode: '',
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals(null, url.searchParams.get('hl'));
  });

  test('SkillsWebview_NullLanguageCode_OmitHl', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      languageCode: null,
    });

    const webviewApp = new TestSkillsWebview();
    const url = new URL(webviewApp.getRemoteUrlForTesting());
    assertEquals(null, url.searchParams.get('hl'));
  });

  test('SkillsWebview_ShowsLoadingPageUntilHandshake', () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      isSkillsEnabled: true,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const webview = document.createElement('webview');
    webview.id = 'webview';
    webview.setAttribute('hidden', '');
    const loadingPage = document.createElement('loading-page');
    loadingPage.id = 'loading-page';
    const errorPage = document.createElement('error-page');
    errorPage.id = 'error-page';
    errorPage.setAttribute('hidden', '');
    document.body.appendChild(webview);
    document.body.appendChild(loadingPage);
    document.body.appendChild(errorPage);

    new SkillsWebview();
    assertFalse(loadingPage.hasAttribute('hidden'));
    assertTrue(errorPage.hasAttribute('hidden'));
    assertTrue(webview.hasAttribute('hidden'));
  });

  test('SkillsWebview_Disabled_ShowsErrorPage', async () => {
    loadTimeData.overrideValues({
      devMode: true,
      isSkillsWebViewV2Enabled: true,
      isSkillsEnabled: false,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const webview = document.createElement('webview');
    webview.id = 'webview';
    const loadingPage = document.createElement('loading-page');
    loadingPage.id = 'loading-page';
    const errorPage = document.createElement('error-page');
    errorPage.id = 'error-page';
    errorPage.setAttribute('hidden', '');
    document.body.appendChild(webview);
    document.body.appendChild(loadingPage);
    document.body.appendChild(errorPage);

    const webviewApp = new SkillsWebview();
    await webviewApp.init();

    assertTrue(loadingPage.hasAttribute('hidden'));
    assertFalse(errorPage.hasAttribute('hidden'));
    assertEquals(ErrorType.SKILLS_DISABLED, errorPage.errorType);
    assertEquals('true', webview.getAttribute('hidden'));
  });

  test('LoadingPage_RendersPageSkeletonByDefault', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const loadingPage = document.createElement('loading-page');
    document.body.appendChild(loadingPage);
    await loadingPage.updateComplete;

    assertTrue(!!loadingPage.shadowRoot?.querySelector('.app-layout'));
    assertEquals(null, loadingPage.shadowRoot?.querySelector('.dialog-layout'));
  });

  test('LoadingPage_RendersDialogSkeletonWhenDialogIsTrue', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const loadingPage = document.createElement('loading-page');
    loadingPage.dialog = true;
    document.body.appendChild(loadingPage);
    await loadingPage.updateComplete;

    assertTrue(!!loadingPage.shadowRoot?.querySelector('.dialog-layout'));
  });

  test('LoadingPage_RendersEditorSkeletonWhenEditorIsTrue', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const loadingPage = document.createElement('loading-page');
    loadingPage.editor = true;
    document.body.appendChild(loadingPage);
    await loadingPage.updateComplete;

    // Look for an editor specific element.
    assertTrue(
        !!loadingPage.shadowRoot?.querySelector('.skeleton-editor-card'));
  });

  test('SkillsWebview_OnUserSkillsUpdated_CallsBridge', () => {
    class BridgeMockSkillsWebview extends SkillsWebview {
      sentSkillsUpdated = false;
      override bridge = {
        sendSkillsUpdated: () => {
          this.sentSkillsUpdated = true;
        },
      } as unknown as SkillsWebviewBridge;
    }

    const webviewApp = new BridgeMockSkillsWebview();
    webviewApp.onUserSkillsUpdated();
    assertTrue(webviewApp.sentSkillsUpdated);
  });
});
