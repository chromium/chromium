// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path:
// chrome/browser/glic/host/glic_api_zss_browsertest.cc

import type {ZeroStateSuggestionsV2} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertTrue, observeSequence, testMain} from './browser_test_base.js';

class ZeroStateSuggestionsApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  private async closePanelAndWaitUntilInactive() {
    assertDefined(this.host.closePanel);
    await this.host.closePanel();
    await observeSequence(this.host.panelActive()).waitForValue(false);
  }

  async testGetZeroStateSuggestionsForFocusedTabApi() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsForFocusedTabFailsWhenHidden() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(0, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachine() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachineImplicitPreservesDisabled() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testNoZssWarmingStateMachineImplicitPreservesEnabled() {
    assertDefined(this.host.getZeroStateSuggestionsForFocusedTab);
    const suggestions = await this.host.getZeroStateSuggestionsForFocusedTab();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
  }

  async testGetZeroStateSuggestionsApi() {
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
    assertEquals(false, suggestions.isPending);
  }

  async testGetZeroStateSuggestionsUnsubscribeAndResubscribe() {
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence1 = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions1 = await sequence1.next();
    assertDefined(suggestions1);
    assertEquals(3, suggestions1.suggestions.length);

    // Unsubscribe.
    sequence1.unsubscribe();

    // Re-subscribing should work and fetch suggestions without hitting a closed
    // pipe.
    const sequence2 = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions2 = await sequence2.next();
    assertDefined(suggestions2);
    assertEquals(3, suggestions2.suggestions.length);
    assertEquals(false, suggestions2.isPending);
  }

  async testGetZeroStateSuggestionsMultipleNavigations() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);
    assertEquals(
        'Sug 1 for /test_data/page.html',
        suggestions.suggestions[0]?.suggestion);
    assertEquals(false, suggestions.isPending);

    // After a second navigation occurs.
    assertTrue(
        await this.browser.navigateActiveTab(this.getTestUrl('page.html?new')));

    // Should first get a pending state.
    const suggestions2 = await sequence.next();
    assertDefined(suggestions2);
    // We don't care about the suggestions here.
    assertEquals(true, suggestions2.isPending);

    // Should later get the actual suggestions.
    const suggestions3 = await sequence.next();
    assertDefined(suggestions3);
    assertEquals(3, suggestions3.suggestions.length);
    assertEquals(
        'Sug 1 for /test_data/page.html?new',
        suggestions3.suggestions[0]?.suggestion);
    assertEquals(false, suggestions3.isPending);
  }

  async testGetZeroStateSuggestionsFailsWhenHidden() {
    // Initial state.
    assertDefined(this.host.getZeroStateSuggestions);
    const sequence = observeSequence<ZeroStateSuggestionsV2>(
        this.host.getZeroStateSuggestions());
    const suggestions = await sequence.next();
    assertDefined(suggestions);
    assertEquals(3, suggestions.suggestions.length);

    // Close panel.
    assertDefined(this.host.closePanel);
    await this.closePanelAndWaitUntilInactive();

    // After next navigation in focused tab occurs.
    await this.advanceToNextStep();
  }
}

testMain([ZeroStateSuggestionsApiTests]);
