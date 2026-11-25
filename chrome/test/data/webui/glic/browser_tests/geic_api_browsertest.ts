// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/gemini_enterprise/geic_api_browsertest.cc

import type {GeicBrowserHost} from '/glic/glic_api/glic_api.js';
import {                     //
  AssistantTarget,           //
  AuthTabPurpose,            //
  CloseAuthTabResult,        //
  CloseSignInTabResult,      //
  HostCapability,            //
  OpenAuthTabResult,         //
  OpenSignInTabResult,       //
  SelectRolloutStageResult,  //
  SwitchAssistantResult,     //
} from '/glic/glic_api/glic_api.js';

import {               //
  ApiTestFixtureBase,  //
  assertDefined,       //
  assertEquals,        //
  assertFalse,         //
  assertTrue,          //
  observeSequence,     //
  testMain,            //
} from './browser_test_base.js';

class GlicGeicApiBrowserTest extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  get geic(): GeicBrowserHost {
    assertDefined(this.host.getGeicClient);
    const geic = this.host.getGeicClient();
    assertDefined(geic);
    return geic;
  }

  async testGeicSetOnboardingCompleted() {
    assertDefined(this.host.isOnboardingCompleted);
    assertDefined(this.host.setOnboardingCompleted);
    const completedSequence =
        observeSequence<boolean>(this.host.isOnboardingCompleted());
    assertFalse(await completedSequence.next());

    // Let the browser check that the GEiC FRE isn't completed yet.
    await this.advanceToNextStep();

    this.host.setOnboardingCompleted();
    assertTrue(await completedSequence.next());
  }

  // GEiC doesn't use the Chrome primary account, so the host API keeps working
  // after Chrome sign-in is paused.
  async testSignInPauseStateGeic() {
    assertDefined(this.host.getUserProfileInfo);
    const profileInfo = await this.host.getUserProfileInfo();
    assertEquals('Glic Testing', profileInfo.displayName);
    // Chrome sign-in is paused before the next step.
    await this.advanceToNextStep();
    const profileInfoAfterPause = await this.host.getUserProfileInfo();
    assertEquals(profileInfo.displayName, profileInfoAfterPause.displayName);
  }

  async testGeicSignInTab() {
    // Closing before opening any tab returns NO_SIGN_IN_TAB.
    const initialResult = await this.geic.closeSignInTab();
    assertEquals(initialResult, CloseSignInTabResult.NO_SIGN_IN_TAB);

    // Calling openSignInTab without a URL returns ERROR_NO_URL.
    const noUrlResult = await this.geic.openSignInTab();
    assertEquals(noUrlResult, OpenSignInTabResult.ERROR_NO_URL);

    // Calling openSignInTab with a disallowed URL returns ERROR_DISALLOWED_URL.
    const disallowedResult = await this.geic.openSignInTab(
        {signinUrl: 'https://user:pass@accounts.google.com/signin'});
    assertEquals(disallowedResult, OpenSignInTabResult.ERROR_DISALLOWED_URL);

    // Call openSignInTab with allowed Google signin URL in options.
    const openResult = await this.geic.openSignInTab(
        {signinUrl: 'https://accounts.google.com/signin'});
    assertEquals(openResult, OpenSignInTabResult.SUCCESS);

    // Close the sign-in tab.
    const result = await this.geic.closeSignInTab();
    assertEquals(result, CloseSignInTabResult.SUCCESS);

    // Calling closeSignInTab again returns NO_SIGN_IN_TAB.
    const secondResult = await this.geic.closeSignInTab();
    assertEquals(secondResult, CloseSignInTabResult.NO_SIGN_IN_TAB);
  }

  async testGeicAuthTab() {
    const geic = this.geic;
    assertDefined(geic.openAuthTab);
    assertDefined(geic.closeAuthTab);
    const purpose = AuthTabPurpose.CONNECTOR_OAUTH;

    // Closing before opening any tab returns NO_AUTH_TAB.
    assertEquals(
        (await geic.closeAuthTab({purpose})).result,
        CloseAuthTabResult.NO_AUTH_TAB);

    // An unknown purpose is rejected.
    assertEquals(
        (await geic.openAuthTab({
          purpose: AuthTabPurpose.UNKNOWN,
          url: 'https://accounts.google.com/signin',
        })).result,
        OpenAuthTabResult.ERROR_INVALID_PURPOSE);

    // Calling openAuthTab without a URL returns ERROR_NO_URL.
    assertEquals(
        (await geic.openAuthTab({purpose})).result,
        OpenAuthTabResult.ERROR_NO_URL);

    // A 3P provider URL must go through the GE redirector.
    assertEquals(
        (await geic.openAuthTab(
             {purpose, url: 'https://login.example.com/authorize'}))
            .result,
        OpenAuthTabResult.ERROR_DISALLOWED_URL);

    // The GE redirector URL is allowed.
    assertEquals(
        (await geic.openAuthTab({
          purpose,
          url: 'https://vertexaisearch.cloud.google.com/oauth-redirect' +
              '?continue_uri=https%3A%2F%2Flogin.example.com%2Fauthorize',
        })).result,
        OpenAuthTabResult.SUCCESS);

    // Closing the sign-in purpose does not affect the connector OAuth tab.
    assertEquals(
        (await geic.closeAuthTab({purpose: AuthTabPurpose.SIGN_IN})).result,
        CloseAuthTabResult.NO_AUTH_TAB);

    // Close the connector OAuth tab.
    assertEquals(
        (await geic.closeAuthTab({purpose})).result,
        CloseAuthTabResult.CLOSED_ACTIVE);

    // Calling closeAuthTab again returns NO_AUTH_TAB.
    assertEquals(
        (await geic.closeAuthTab({purpose})).result,
        CloseAuthTabResult.NO_AUTH_TAB);
  }
  // `window.open()` from the GEiC guest opens a plain tab that the web client
  // can't reach: it returns no window handle, so a popup-based auth flow can't
  // observe or close it. Auth flows must use `openAuthTab()`, whose tab Chrome
  // tracks and can close on the client's behalf.
  async testGeicWindowOpenReturnsNullButAuthTabIsTracked() {
    const popup = window.open('https://www.chromium.org', '_blank');
    assertTrue(popup === null, 'window.open() should return null');
    // Let the browser check that `window.open()` still opened an untracked
    // tab.
    await this.advanceToNextStep();

    const geic = this.geic;
    assertDefined(geic.openAuthTab);
    assertDefined(geic.closeAuthTab);
    const purpose = AuthTabPurpose.SIGN_IN;
    assertEquals(
        (await geic.openAuthTab(
             {purpose, url: 'https://accounts.google.com/signin'}))
            .result,
        OpenAuthTabResult.SUCCESS);
    // Let the browser check that the auth tab opened.
    await this.advanceToNextStep();

    assertEquals(
        (await geic.closeAuthTab({purpose})).result,
        CloseAuthTabResult.CLOSED_ACTIVE);
  }

  async testGeicSwitchAssistantNotAllowed() {
    assertDefined(this.host.getHostCapabilities);
    assertDefined(this.host.switchAssistant);
    assertFalse(
        this.host.getHostCapabilities().has(HostCapability.SWITCH_ASSISTANT));

    // An unknown target is rejected.
    assertEquals(
        (await this.host.switchAssistant({target: AssistantTarget.UNKNOWN}))
            .result,
        SwitchAssistantResult.ERROR_INVALID_TARGET);

    assertEquals(
        (await this.host.switchAssistant({
          target: AssistantTarget.GEMINI_CONSUMER,
        })).result,
        SwitchAssistantResult.ERROR_NOT_ALLOWED);

    assertEquals(
        (await this.host.switchAssistant({
          target: AssistantTarget.GEMINI_ENTERPRISE,
        })).result,
        SwitchAssistantResult.ERROR_NOT_ALLOWED);
  }

  async testGeicSwitchAssistant() {
    assertDefined(this.host.getHostCapabilities);
    assertDefined(this.host.switchAssistant);
    assertTrue(
        this.host.getHostCapabilities().has(HostCapability.SWITCH_ASSISTANT));

    assertEquals(
        (await this.host.switchAssistant({
          target: AssistantTarget.GEMINI_CONSUMER,
        })).result,
        SwitchAssistantResult.SUCCESS);
  }

  async testSwitchAssistantFromConsumer() {
    assertDefined(this.host.getHostCapabilities);
    assertDefined(this.host.switchAssistant);
    assertFalse(
        this.host.getHostCapabilities().has(HostCapability.SWITCH_ASSISTANT));

    assertEquals(
        (await this.host.switchAssistant({
          target: AssistantTarget.GEMINI_CONSUMER,
        })).result,
        SwitchAssistantResult.ERROR_NOT_ALLOWED);

    assertEquals(
        (await this.host.switchAssistant({
          target: AssistantTarget.GEMINI_ENTERPRISE,
        })).result,
        SwitchAssistantResult.ERROR_NOT_ALLOWED);
  }

  async testGeicRolloutStagesSuppressedBySwitch() {
    const geic = this.geic;
    assertDefined(geic.getRolloutStages);
    assertDefined(geic.selectRolloutStage);

    const info = await geic.getRolloutStages();
    assertEquals(info.stages.length, 0);
    assertEquals(info.currentStageId, undefined);

    assertEquals(
        (await geic.selectRolloutStage({stageId: 'autopush'})).result,
        SelectRolloutStageResult.NOT_SUPPORTED);
  }
}

class GlicGeicRolloutStageApiBrowserTest extends GlicGeicApiBrowserTest {
  async testGeicRolloutStages() {
    const geic = this.geic;
    assertDefined(geic.getRolloutStages);
    assertDefined(geic.selectRolloutStage);

    const initialInfo = await geic.getRolloutStages();
    assertEquals(initialInfo.stages.length, 3);
    assertEquals(initialInfo.stages[0]!.id, 'autopush');
    assertEquals(initialInfo.stages[0]!.label, 'Autopush');
    assertEquals(initialInfo.stages[1]!.id, 'staging');
    assertEquals(initialInfo.stages[1]!.label, 'Staging');
    assertEquals(initialInfo.stages[2]!.id, 'prod');
    assertEquals(initialInfo.stages[2]!.label, 'Prod');
    assertEquals(initialInfo.currentStageId, 'staging');

    assertEquals(
        (await geic.selectRolloutStage({stageId: 'unknown_stage'})).result,
        SelectRolloutStageResult.UNKNOWN_STAGE);

    await this.advanceToNextStep();

    assertEquals(
        (await geic.selectRolloutStage({stageId: 'staging'})).result,
        SelectRolloutStageResult.SUCCESS);

    await this.advanceToNextStep();

    assertEquals(
        (await geic.selectRolloutStage({stageId: 'autopush'})).result,
        SelectRolloutStageResult.SUCCESS);

    // Before the browser actually restarts, `currentStageId` continues to
    // reflect the session's latched stage ('staging').
    const afterSelectInfo = await geic.getRolloutStages();
    assertEquals(afterSelectInfo.currentStageId, 'staging');
  }
}

class GlicGeicPinOnBindApiBrowserTest extends GlicGeicApiBrowserTest {
  async testGeicCreateTabPinOnBind() {
    assertDefined(this.host.createTab);
    assertDefined(this.host.getPinnedTabs);
    const pinnedTabs = observeSequence(this.host.getPinnedTabs());
    await pinnedTabs.waitFor(tabs => tabs.length === 1);

    const pageUrl = new URL('/glic/test_data/page.html', location.href).href;
    const pinnedTab = await this.host.createTab(pageUrl, {});
    const afterCreateTabTabs =
        await pinnedTabs.waitFor(tabs => tabs.length === 2);
    assertTrue(afterCreateTabTabs.some(tab => tab.tabId === pinnedTab.tabId));
  }

  async testGeicLinkClickPinOnBind() {
    assertDefined(this.host.getPinnedTabs);
    const pinnedTabs = observeSequence(this.host.getPinnedTabs());
    await pinnedTabs.waitFor(tabs => tabs.length === 1);

    const link = document.createElement('a');
    link.setAttribute(
        'href', new URL('/glic/test_data/page.html', location.href).href);
    link.setAttribute('target', '_blank');
    document.body.appendChild(link);
    link.click();

    const afterLinkTabs = await pinnedTabs.waitFor(tabs => tabs.length === 2);
    assertEquals(afterLinkTabs.length, 2);
  }

  async testGeicAuthTabDoesNotPin() {
    const geic = this.geic;
    assertDefined(geic.openAuthTab);
    assertDefined(geic.closeAuthTab);
    assertEquals(
        (await geic.openAuthTab({
          purpose: AuthTabPurpose.SIGN_IN,
          url: 'https://accounts.google.com/signin',
        })).result,
        OpenAuthTabResult.SUCCESS);

    await this.advanceToNextStep();

    assertEquals(
        (await geic.closeAuthTab({purpose: AuthTabPurpose.SIGN_IN})).result,
        CloseAuthTabResult.CLOSED_ACTIVE);
    assertEquals(
        (await geic.openAuthTab({
          purpose: AuthTabPurpose.CONNECTOR_OAUTH,
          url: 'https://vertexaisearch.cloud.google.com/oauth-redirect' +
              '?continue_uri=https%3A%2F%2Flogin.example.com%2Fauthorize',
        })).result,
        OpenAuthTabResult.SUCCESS);
  }
}

testMain([
  GlicGeicApiBrowserTest,
  GlicGeicRolloutStageApiBrowserTest,
  GlicGeicPinOnBindApiBrowserTest,
]);
