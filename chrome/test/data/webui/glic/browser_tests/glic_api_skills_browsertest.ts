// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path:
// chrome/browser/glic/host/glic_api_skills_browsertest.cc

import {SkillSource, SkillsWebClientEvent} from '/glic/glic_api/glic_api.js';

import {ApiTestFixtureBase, assertDefined, assertEquals, assertRejects, assertTrue, assertUndefined, observeSequence, testMain} from './browser_test_base.js';

class SkillsApiTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testGetSkillSuccess() {
    assertDefined(this.host.skills);
    const skillsApi = await observeSequence(this.host.skills()).next();
    assertDefined(skillsApi);
    assertDefined(skillsApi.getSkillPreviews);
    assertDefined(skillsApi.getSkill);
    const skillPreviewsSequence = observeSequence(skillsApi.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    const targetSkill = skills.find(s => s.name === 'test_skill_1');
    assertDefined(targetSkill);
    const actualSkill = await skillsApi.getSkill(targetSkill.id);
    assertDefined(actualSkill);
    assertEquals(actualSkill.preview.id, targetSkill.id);
    assertEquals(actualSkill.preview.name, 'test_skill_1');
    assertEquals(actualSkill.preview.icon, 'test_icon_1');
    assertEquals(actualSkill.prompt, 'test_prompt_1');
    assertEquals(actualSkill.sourceSkillId, 'source_id_1');
  }

  async testGetSkillPreviewsSuccess() {
    assertDefined(this.host.getSkillPreviews);
    assertDefined(this.host.getSkill);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    const skill1 = skills.find(s => s.name === 'test_skill_1');
    assertDefined(skill1);
    assertEquals('test_icon_1', skill1.icon);
    assertTrue(skill1.creationTime instanceof Date);
    const actualSkill1 = await this.host.getSkill(skill1.id);
    assertDefined(actualSkill1);
    assertEquals(actualSkill1.sourceSkillId, 'source_id_1');
    assertEquals(
        actualSkill1.preview.creationTime?.getTime(),
        skill1.creationTime.getTime());
    const skill2 = skills.find(s => s.name === 'test_skill_2');
    assertDefined(skill2);
    assertEquals('test_icon_2', skill2.icon);
    assertTrue(skill2.creationTime instanceof Date);
    const actualSkill2 = await this.host.getSkill(skill2.id);
    assertDefined(actualSkill2);
    assertEquals(actualSkill2.sourceSkillId, 'source_id_2');
    assertEquals(
        actualSkill2.preview.creationTime?.getTime(),
        skill2.creationTime.getTime());
  }

  async testGetSkillDisabled() {
    // Check that skills are disabled via the new API
    assertDefined(this.host.skills);
    assertUndefined(await observeSequence(this.host.skills()).next());

    // API should be gone when disabled.
    assertUndefined(this.host.getSkill);
    assertUndefined(this.host.createSkill);
    assertUndefined(this.host.updateSkill);
    assertUndefined(this.host.showManageSkillsUi);
    assertUndefined(this.host.showBrowseSkillsUi);
    assertUndefined(this.host.recordSkillsWebClientEvent);
    assertUndefined(this.host.getSkillPreviews);
    assertUndefined(this.host.getSkillToInvoke);
  }

  async testSendingContextualSkillsToGlic() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    let skills = await skillPreviewsSequence.waitFor(s => s.length === 2);
    let user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    let user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    await this.advanceToNextStep();

    skills = await skillPreviewsSequence.waitFor(s => s.length === 4);
    const contextual_skill_1 =
        skills.find(s => s.id === 'contextual_skill_id_1');
    assertDefined(contextual_skill_1);
    assertEquals('contextual_skill_1', contextual_skill_1.name);
    assertEquals(
        'contextual_skill_description_1', contextual_skill_1.description);
    const contextual_skill_2 =
        skills.find(s => s.id === 'contextual_skill_id_2');
    assertDefined(contextual_skill_2);
    assertEquals('contextual_skill_2', contextual_skill_2.name);
    assertEquals(
        'contextual_skill_description_2', contextual_skill_2.description);
    user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    assertEquals(true, contextual_skill_1.isContextual);
    assertEquals(true, contextual_skill_2.isContextual);
    assertEquals(false, user_skill_1.isContextual);
    assertEquals(false, user_skill_2.isContextual);
    await this.advanceToNextStep();

    skills = await skillPreviewsSequence.waitFor(s => s.length === 3);
    const contextual_skill_3 =
        skills.find(s => s.id === 'contextual_skill_id_3');
    assertDefined(contextual_skill_3);
    assertEquals('contextual_skill_3', contextual_skill_3.name);
    assertEquals(
        'contextual_skill_description_3', contextual_skill_3.description);
    user_skill_1 = skills.find(s => s.name === 'user_skill_1');
    assertDefined(user_skill_1);
    user_skill_2 = skills.find(s => s.name === 'user_skill_2');
    assertDefined(user_skill_2);
    assertEquals(true, contextual_skill_3.isContextual);
    assertEquals(false, user_skill_1.isContextual);
    assertEquals(false, user_skill_2.isContextual);
  }

  async testSendingPendingContextualSkillsToGlic() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.waitFor(s => s.length === 1);
    const contextual_skill_1 =
        skills.find(s => s.id === 'contextual_skill_id_1');
    assertDefined(contextual_skill_1);
    assertEquals('contextual_skill_1', contextual_skill_1.name);
    assertEquals(
        'contextual_skill_description_1', contextual_skill_1.description);
    assertEquals(true, contextual_skill_1.isContextual);
  }

  async testChangingActiveTabClearsPendingContextualSkills() {
    assertDefined(this.host.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(this.host.getSkillPreviews());
    const skills = await skillPreviewsSequence.next();
    assertEquals(0, skills.length);
  }
}

// TODO(b/546606964): enable these tests on android.
class SkillsDesktopOnlyApiTests extends SkillsApiTests {
  async testSkillsEnabledToggledAtRuntime() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    // 1. Initially disabled.
    assertUndefined(await skillsSequence.next());

    // 2. Enable skills pref at runtime.
    await this.advanceToNextStep();
    const enabledSkills = await skillsSequence.next();
    assertDefined(enabledSkills);

    // 3. Disable skills pref at runtime.
    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());
  }

  async testContextualSkillsRetainedWhenStartingPrefDisabled() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    // Initially disabled.
    assertUndefined(await skillsSequence.next());

    // Step 1: Enable skills pref at runtime and verify cached contextual skills
    // are received.
    await this.advanceToNextStep();
    const enabledSkills = await skillsSequence.next();
    assertDefined(enabledSkills);
    assertDefined(enabledSkills.getSkillPreviews);

    const previewsSeq = observeSequence(enabledSkills.getSkillPreviews());
    const previews = await previewsSeq.waitFor(s => s.length === 1);
    assertEquals('contextual_skill_id_1', previews[0]?.id);
    assertEquals('contextual_skill_1', previews[0]?.name);
  }

  async testSkillsEnabledState() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    const skills = await skillsSequence.next();
    assertDefined(skills);

    // Call when enabled
    assertDefined(skills.getSkill);
    await assertRejects(skills.getSkill('non-existent-id'));

    // Get a valid skill ID from getSkillPreviews.
    assertDefined(skills.getSkillPreviews);
    const skillPreviewsSequence = observeSequence(skills.getSkillPreviews());
    const skillPreviews =
        await skillPreviewsSequence.waitFor(s => s.length === 1);
    const skillId = skillPreviews[0]!.id;

    // Verify that both the new API and deprecated API succeed when skills are
    // enabled.
    assertDefined(skills.recordSkillsWebClientEvent);
    skills.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);

    assertDefined(skills.getSkill);
    const skillFromNewApi = await skills.getSkill(skillId);
    assertDefined(skillFromNewApi);
    assertEquals('source_id_1', skillFromNewApi.sourceSkillId);

    assertDefined(this.host.getSkill);
    const skillFromDeprecatedApi = await this.host.getSkill(skillId);
    assertDefined(skillFromDeprecatedApi);
    assertEquals('source_id_1', skillFromDeprecatedApi.sourceSkillId);
    assertDefined(this.host.getSkillToInvoke);

    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());

    // When skills are disabled, API methods that return a Promise should reject
    // with an error, both when calling via a saved reference to
    // GlicBrowserSkills (new API)...
    assertDefined(skills.recordSkillsWebClientEvent);
    skills.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);
    assertDefined(skills.getSkill);
    await assertRejects(skills.getSkill(skillId));
    assertDefined(skills.createSkill);
    await assertRejects(skills.createSkill({prompt: 'test'}));
    assertDefined(skills.updateSkill);
    await assertRejects(skills.updateSkill({id: skillId}));

    // ...and when calling via GlicBrowserHost (deprecated API).
    assertDefined(this.host.recordSkillsWebClientEvent);
    this.host.recordSkillsWebClientEvent(SkillsWebClientEvent.OPENED_MENU);
    assertDefined(this.host.getSkill);
    await assertRejects(this.host.getSkill!(skillId));
    assertDefined(this.host.createSkill);
    await assertRejects(this.host.createSkill!({prompt: 'test'}));
    assertDefined(this.host.updateSkill);
    await assertRejects(this.host.updateSkill!({id: skillId}));

    // Synchronous void functions that couldn't throw an error previously must
    // fail silently without throwing an error, both on GlicBrowserSkills (new
    // API) and on GlicBrowserHost (deprecated API).
    assertDefined(skills.showManageSkillsUi);
    skills.showManageSkillsUi!();
    assertDefined(skills.showBrowseSkillsUi);
    skills.showBrowseSkillsUi!();
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi!();
    assertDefined(this.host.showBrowseSkillsUi);
    this.host.showBrowseSkillsUi!();

    // Advance to next step (re-enable skills) and verify skills observable
    // emits a new instance.
    await this.advanceToNextStep();
    const reenabledSkills = await skillsSequence.next();
    assertDefined(reenabledSkills);
    assertDefined(reenabledSkills.getSkill);
    const reenabledSkill = await reenabledSkills.getSkill(skillId);
    assertDefined(reenabledSkill);
    assertEquals('source_id_1', reenabledSkill.sourceSkillId);
  }

  async testCreateSkillAndDisable() {
    assertDefined(this.host.skills);
    const skillsSequence = observeSequence(this.host.skills());
    const skills = await skillsSequence.next();
    assertDefined(skills);
    assertDefined(skills.createSkill);

    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    await skills.createSkill(request);

    // Advance to step 2 where C++ disables skills and closes the dialog.
    await this.advanceToNextStep();
    assertUndefined(await skillsSequence.next());
    await assertRejects(skills.createSkill(request));
  }

  async testDisplaySkillInDialogSuccess() {
    assertDefined(this.host.createSkill);
    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    this.host.createSkill(request);
  }

  async testShowManageSkillsUi() {
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi();
  }

  async testShowBrowseSkillsUi() {
    assertDefined(this.host.showBrowseSkillsUi);
    this.host.showBrowseSkillsUi();
  }

  async testShowManageSkillsUiNoWindow() {
    assertDefined(this.host.showManageSkillsUi);
    this.host.showManageSkillsUi();
  }

  async testCreateSkillNoWindow() {
    assertDefined(this.host.createSkill);
    const request = {
      id: 'id',
      name: 'name',
      icon: 'icon',
      prompt: 'prompt',
      source: SkillSource.FIRST_PARTY,
    };
    this.host.createSkill(request);
  }
}

testMain([SkillsApiTests, SkillsDesktopOnlyApiTests]);
