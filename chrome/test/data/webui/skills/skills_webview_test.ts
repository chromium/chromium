// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from '//resources/js/load_time_data.js';
import {SkillsDialogType} from 'chrome://skills/skill.mojom-webui.js';
import {SkillsWebview} from 'chrome://skills/v2/skills_webview.js';
import {IS_FIRST_PARTY_QUERY_PARAMETER, IS_SAVING_GEMINI_QUERY_PARAMETER} from 'chrome://skills/v2/skills_webview_bridge_constants.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

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
    assertEquals('true', url.searchParams.get(IS_FIRST_PARTY_QUERY_PARAMETER));
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
    assertEquals(null, url.searchParams.get(IS_FIRST_PARTY_QUERY_PARAMETER));
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
});
